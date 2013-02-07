/*  =========================================================================
    zre_udp - UDP management class

    -------------------------------------------------------------------------
    Copyright (c) 1991-2012 iMatix Corporation <www.imatix.com>
    Copyright other contributors as noted in the AUTHORS file.

    This file is part of Zyre, an open-source framework for proximity-based
    peer-to-peer applications -- See http://zyre.org.

    This is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 3 of the License, or (at
    your option) any later version.

    This software is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this program. If not, see
    <http://www.gnu.org/licenses/>.
    =========================================================================
*/

#include <czmq.h>
#include "../include/zre_internal.hpp"

#include "../include/platform.hpp"
#if defined (HAVE_LINUX_WIRELESS_H)
#   include <linux/wireless.h>
#else
#   if defined (HAVE_NET_IF_H)
#       include <net/if.h>
#   endif
#   if defined (HAVE_NET_IF_MEDIA_H)
#       include <net/if_media.h>
#   endif
#endif
#if defined (__WINDOWS__)
#   if (_WIN32_WINNT >= 0x0501)
#   else
#       undef _WIN32_WINNT
#       define _WIN32_WINNT 0x0501
#   endif
#   include <ws2tcpip.h>           // getnameinfo()
#   include <iphlpapi.h>           // GetAdaptersAddresses()
#endif
#if defined (__APPLE__)
#		define ENONET	EHOSTDOWN
#endif

//  Local functions

static void
    s_handle_io_error (char *reason);
static bool
    s_wireless_nic (const char* name);
static void
    s_get_interface (zre_udp_data_t *self);


//  -----------------------------------------------------------------
//  UDP instance

struct zre_udp_data_t {
    int handle;                 //  Socket for send/recv
    int port_nbr;               //  UDP port number we work on
    struct sockaddr_in address;     //  Own address
    struct sockaddr_in broadcast;   //  Broadcast address
    struct sockaddr_in sender;      //  Where last recv came from
    char *host;                 //  Our own address as string
    char *from;                 //  Sender address of last message
};

//  -----------------------------------------------------------------
//  Constructor

zre_udp::zre_udp (int port_nbr)
{
    myData = new zre_udp_data_t;
    myData->port_nbr = port_nbr;

    //  Create UDP socket
    myData->handle = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (myData->handle == ZRE_INVALID_SOCKET)
        s_handle_io_error ("socket");

    //  Ask operating system to let us do broadcasts from socket
    int on = 1;
    if (setsockopt (myData->handle, SOL_SOCKET, SO_BROADCAST,
                   (char *) &on, sizeof (on)) == -1)
        s_handle_io_error ("setsockopt (SO_BROADCAST)");

    //  Allow multiple processes to bind to socket; incoming
    //  messages will come to each process
    if (setsockopt (myData->handle, SOL_SOCKET, SO_REUSEADDR,
                   (char *) &on, sizeof (on)) == -1)
        s_handle_io_error ("setsockopt (SO_REUSEADDR)");

#if defined (SO_REUSEPORT)
    if (setsockopt (myData->handle, SOL_SOCKET, SO_REUSEPORT,
                    (char *) &on, sizeof (on)) == -1)
        s_handle_io_error ("setsockopt (SO_REUSEPORT)");
#endif
    //  PROBLEM: this design will not survive the network interface being
    //  killed and restarted while the program is running.
    struct sockaddr_in sockaddr = { 0 };
    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = htons (myData->port_nbr);
    sockaddr.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (myData->handle, (struct sockaddr *) &sockaddr, sizeof (sockaddr)) == -1)
        s_handle_io_error ("bind");

    //  Get the network interface (not portable)
    s_get_interface (myData);
    
    //  Now get printable address as host name
    myData->host = new char[INET_ADDRSTRLEN];
    getnameinfo ((struct sockaddr *) &myData->address, sizeof (myData->address),
                 myData->host, INET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
}


//  -----------------------------------------------------------------
//  Destructor

zre_udp::~zre_udp ()
{
#ifndef __WINDOWS__
	_close (myData->handle);
#else
	closesocket(myData->handle);
#endif
	myData->handle = ZRE_INVALID_SOCKET;
    delete[] myData->host;
    delete[] myData->from;
    delete myData;
}


//  -----------------------------------------------------------------
//  Returns UDP socket handle

int
zre_udp::handle ()
{
    return myData->handle;
}


//  -----------------------------------------------------------------
//  Send message using UDP broadcast

void
zre_udp::send (byte *buffer, size_t length)
{
    myData->broadcast.sin_addr.s_addr = INADDR_BROADCAST;
    ssize_t size = sendto (myData->handle, (char *) buffer, length, 0,
        (struct sockaddr *) &myData->broadcast, sizeof (struct sockaddr_in));
    if (size == -1)
        s_handle_io_error ("sendto");
}


//  -----------------------------------------------------------------
//  Receive message from UDP broadcast
//  Returns size of received message, or -1

ssize_t
zre_udp::recv (byte *buffer, size_t length)
{
    socklen_t si_len = sizeof (struct sockaddr_in);
    ssize_t size = recvfrom (myData->handle, (char *) buffer, length, 0,
        (struct sockaddr *) &myData->sender, &si_len);
    if (size == -1)
        s_handle_io_error ("recvfrom");

    //  Store sender address as printable string
    myData->from = new char[INET_ADDRSTRLEN];
#if (defined (__WINDOWS__))
    getnameinfo ((struct sockaddr *) &myData->sender, si_len,
                 myData->from, INET_ADDRSTRLEN, NULL, 0, NI_NUMERICHOST);
#else
    inet_ntop (AF_INET, &myData->sender.sin_addr, myData->from, si_len);
#endif
    return size;
}


//  -----------------------------------------------------------------
//  Return our own IP address as printable string

char *
zre_udp::host ()
{
    return myData->host;
}


//  -----------------------------------------------------------------
//  Return IP address of peer that sent last message

char *
zre_udp::from ()
{
    return myData->from;
}

//  -----------------------------------------------------------------
//  Handle error from I/O operation

static void
s_handle_io_error (char *reason)
{
#if defined (__WINDOWS__)
    switch (WSAGetLastError ()) {
        case WSAEINTR:        errno = EINTR;      break;
        case WSAEBADF:        errno = EBADF;      break;
        case WSAEWOULDBLOCK:  errno = EAGAIN;     break;
        case WSAEINPROGRESS:  errno = EAGAIN;     break;
        case WSAENETDOWN:     errno = ENETDOWN;   break;
        case WSAECONNRESET:   errno = ECONNRESET; break;
        case WSAECONNABORTED: errno = EPIPE;      break;
        case WSAESHUTDOWN:    errno = ECONNRESET; break;
        case WSAEINVAL:       errno = EPIPE;      break;
        default:              errno = GetLastError ();
    }
#endif
    if (errno == EAGAIN
    ||  errno == ENETDOWN
    ||  errno == EHOSTUNREACH
    ||  errno == ENETUNREACH
    ||  errno == EINTR
#if !defined (__WINDOWS__)
    ||  errno == EPROTO
    ||  errno == ENOPROTOOPT
    ||  errno == EHOSTDOWN
    ||  errno == ENONET
    ||  errno == EOPNOTSUPP
    ||  errno == EWOULDBLOCK
#endif
    )
        return;             //  Ignore error and try again
    else
    if (errno == EPIPE
    ||  errno == ECONNRESET)
        return;             //  Ignore error and try again
    else {
        zclock_log ("E: (UDP) error '%s' on %s", strerror (errno), reason);
        assert (false);
    }
}

//  Check if given NIC name is wireless

static bool
s_wireless_nic (const char* name)
{
    int sock = 0;
    bool result = FALSE;

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return FALSE;

#if defined (SIOCGIFMEDIA)
    struct ifmediareq ifmr;
    memset (&ifmr, 0, sizeof (struct ifmediareq));
    strncpy(ifmr.ifm_name, name, sizeof (ifmr.ifm_name));
    int res = ioctl (sock, SIOCGIFMEDIA, (caddr_t) &ifmr);
    if (res != -1)
        result = (IFM_TYPE (ifmr.ifm_current) == IFM_IEEE80211);

#elif defined (SIOCGIWNAME)
    struct iwreq wrq;
    memset (&wrq, 0, sizeof (struct iwreq));
    strncpy (wrq.ifr_name, name, sizeof (wrq.ifr_name));
    int res = ioctl (sock, SIOCGIWNAME, (caddr_t) &wrq);
    if (res != -1)
        result = TRUE;

#endif
    _close (sock);
    return result;
}

static void
s_get_interface (zre_udp_data_t *myData)
{
#if defined (__UNIX__)
#   if defined (HAVE_GETIFADDRS) && defined (HAVE_FREEIFADDRS)
    struct ifaddrs *interfaces;
    if (getifaddrs (&interfaces) == 0) {
        struct ifaddrs *interface = interfaces;
        while (interface) {
            //  Hopefully the last interface will be WiFi or Ethernet
            if (interface->ifa_addr->sa_family == AF_INET) {
                myData->address = *(struct sockaddr_in *) interface->ifa_addr;
                myData->broadcast = *(struct sockaddr_in *) interface->ifa_broadaddr;
                myData->broadcast.sin_port = htons (myData->port_nbr);
                if (s_wireless_nic (interface->ifa_name))
                    break;
            }
            interface = interface->ifa_next;
        }
    }
    freeifaddrs (interfaces);
#   else
    struct ifreq ifr;
    memset (&ifr, 0, sizeof (ifr));

#   if !defined (LIBZRE_HAVE_ANDROID)
    //  TODO: Using hardcoded wlan0 is ugly
    if (!s_wireless_nic ("wlan0"))
        s_handle_io_error ("wlan0_not_exist");
#   endif

    int sock = 0;
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        s_handle_io_error ("foo_socket_not_opened");

    //  Get interface address
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy (ifr.ifr_name, "wlan0", sizeof (ifr.ifr_name));
    int rc = ioctl (sock, SIOCGIFADDR, (caddr_t) &ifr, sizeof (struct ifreq));
    if (rc == -1)
        s_handle_io_error ("siocgifaddr");

    //  Get interface broadcast address
    memcpy (&myData->address, ((struct sockaddr_in *) &ifr.ifr_addr),
        sizeof (struct sockaddr_in));
    rc = ioctl (sock, SIOCGIFBRDADDR, (caddr_t) &ifr, sizeof (struct ifreq));
    if (rc == -1)
        s_handle_io_error ("siocgifbrdaddr");

    memcpy (&myData->broadcast, ((struct sockaddr_in *) &ifr.ifr_broadaddr),
        sizeof (struct sockaddr_in));
    myData->broadcast.sin_port = htons (myData->port_nbr);
    close (sock);
#   endif

#   elif defined (__WINDOWS__)
    //  Currently does not filter for wireless NIC
    ULONG addr_size = 0;
    DWORD rc = GetAdaptersAddresses (AF_INET,
        GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &addr_size);
    assert (rc == ERROR_BUFFER_OVERFLOW);

    PIP_ADAPTER_ADDRESSES pip_addresses = (PIP_ADAPTER_ADDRESSES)malloc (addr_size);
    rc = GetAdaptersAddresses (AF_INET,
        GAA_FLAG_INCLUDE_PREFIX, NULL, pip_addresses, &addr_size);
    assert (rc == NO_ERROR);

    PIP_ADAPTER_ADDRESSES cur_address = pip_addresses;
    while (cur_address) {
        PIP_ADAPTER_UNICAST_ADDRESS pUnicast = cur_address->FirstUnicastAddress;
        PIP_ADAPTER_PREFIX pPrefix = cur_address->FirstPrefix;

        if (pUnicast && pPrefix) {
            //  myData->broadcast.sin_addr is replaced with 255.255.255.255 in zre_udp_send()
            myData->address = *(struct sockaddr_in *)(pUnicast->Address.lpSockaddr);
            myData->broadcast = *(struct sockaddr_in *)(pPrefix->Address.lpSockaddr);
            myData->broadcast.sin_addr.s_addr |= htonl ((1 << (32 - pPrefix->PrefixLength)) - 1);
        }
        myData->broadcast.sin_port = htons (myData->port_nbr);
        cur_address = cur_address->Next;
    }
    free (pip_addresses);
#   else
#       error "Interface detection TBD on this operating system"
#   endif
}

