/*  =========================================================================
    zre_node - node on a ZRE network

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

//  Optional global context for zre_node instances
ZRE_EXPORT zctx_t *zre_global_ctx = nullptr;
//  Optional temp directory; set by caller if needed
ZRE_EXPORT char *zre_global_tmpdir = nullptr;

//  ---------------------------------------------------------------------
//  Structure of our class
struct zre_node_data_t {
    zctx_t *ctx;                //  Our context wrapper
    bool ctx_owned;             //  True if we created the context
    void *pipe;                 //  Pipe through to agent
};

//  =====================================================================
//  Synchronous part, works in our application thread

//  This is the thread that handles our real node class
static void
    zre_node_agent (void *args, zctx_t *ctx, void *pipe);


//  ---------------------------------------------------------------------
//  Constructor
zre_node::zre_node ()
{
    myData = new zre_node_data_t;
	//  If caller set a default ctx use that, else create our own
    if (zre_global_ctx)
        myData->ctx = zre_global_ctx;
    else {
        myData->ctx = zctx_new ();
        myData->ctx_owned = true;
    }
    myData->pipe = zthread_fork (myData->ctx, zre_node_agent, nullptr);
}


//  ---------------------------------------------------------------------
//  Destructor
zre_node::~zre_node ()
{
    if (myData->ctx_owned)
        zctx_destroy (&myData->ctx);
	delete myData;
}

//  ---------------------------------------------------------------------
//  Receive next message from node
//  Returns zmsg_t object, or NULL if interrupted

zmsg_t *
zre_node::recv ()
{
    zmsg_t *msg = zmsg_recv (myData->pipe);
    return msg;
}


//  ---------------------------------------------------------------------
//  Join a group

int
zre_node::join (const char *group)
{
    zstr_sendm (myData->pipe, "JOIN");
    zstr_send  (myData->pipe, group);
    return 0;
}


//  ---------------------------------------------------------------------
//  Leave a group

int
zre_node::leave (const char *group)
{
    zstr_sendm (myData->pipe, "LEAVE");
    zstr_send  (myData->pipe, group);
    return 0;
}


//  ---------------------------------------------------------------------
//  Send message to single peer; peer ID is first frame in message
//  Destroys message after sending

int
zre_node::whisper (zmsg_t **msg_p)
{
    zstr_sendm (myData->pipe, "WHISPER");
    zmsg_send (msg_p, myData->pipe);
    return 0;
}


//  ---------------------------------------------------------------------
//  Send message to a group of peers

int
zre_node::shout (zmsg_t **msg_p)
{
    zstr_sendm (myData->pipe, "SHOUT");
    zmsg_send (msg_p, myData->pipe);
    return 0;
}


//  ---------------------------------------------------------------------
//  Return node handle, for polling

void *
zre_node::handle () const
{    
    return myData->pipe;
}


//  ---------------------------------------------------------------------
//  Set node header value

void
zre_node::header_set (char *name, char *format, ...)
{
    va_list argptr;
    va_start (argptr, format);
    char *value = (char *) malloc (255 + 1);
    vsnprintf (value, 255, format, argptr);
    va_end (argptr);
    
    zstr_sendm (myData->pipe, "SET");
    zstr_sendm (myData->pipe, name);
    zstr_send  (myData->pipe, value);
    free (value);
}


//  ---------------------------------------------------------------------
//  Publish file under some logical name
//  Physical name is the actual file location

void
zre_node::publish (char *logical, char *physical)
{
    zstr_sendm (myData->pipe, "PUBLISH");
    zstr_sendm (myData->pipe, logical);
    zstr_send  (myData->pipe, physical);
}

//  ---------------------------------------------------------------------
//  Retract published file

void
zre_node::retract (char *logical)
{
    zstr_sendm (myData->pipe, "RETRACT");
    zstr_send  (myData->pipe, logical);
}


//  ---------------------------------------------------------------------
//  Get temporary directory of INBOX and OUTBOX

static char *
s_tmpdir ()
{
    static char our_tmpdir [255] = { 0 };
    if (!zre_global_tmpdir) {
        zre_global_tmpdir = our_tmpdir;
        char *tmp_env = getenv ("TMPDIR");
        if (!tmp_env)
#ifdef __WINDOWS__	
			GetTempPath(255, our_tmpdir);
#else
            tmp_env = "/tmp";
        strcat (zre_global_tmpdir, tmp_env);
#endif

        if (zre_global_tmpdir [strlen (zre_global_tmpdir) -1] != '/')
            strcat (zre_global_tmpdir, "/");
        strcat (zre_global_tmpdir, "zyre");
    }
    return zre_global_tmpdir;
}


//  =====================================================================
//  Asynchronous part, works in the background

//  Beacon frame has this format:
//
//  Z R E       3 bytes
//  version     1 byte, %x01
//  UUID        16 bytes
//  port        2 bytes in network order

const char* BEACON_PROTOCOL   =  "ZRE";
const byte  BEACON_VERSION    =  0x01;

typedef struct {
    byte protocol [3];
    byte version;
    byte uuid [ZRE_UUID_LEN];
    uint16_t port;
} beacon_t;


//  This structure holds the context for our agent, so we can
//  pass that around cleanly to methods which need it

class agent
{
public:
	agent(zctx_t *ctx, void *pipe);
	~agent();

	int recv_from_api();
	zre_peer* s_require_peer (char *identity, char *address, uint16_t port);
	zre_group* s_require_peer_group (char *name);
	zre_group* s_join_peer_group (zre_peer *peer, char *name);
	zre_group* s_leave_peer_group (zre_peer *peer, char *name);
	int recv_from_peer ();
	void beacon_send ();
	int recv_udp_beacon ();
	int recv_fmq_event ();
	int ping_peer (const char *key, void *item);

	void* get_pipe(){return pipe;}
	zre_udp* get_udp(){return udp;}
	fmq_client_t* get_fmq_client(){return fmq_client;}
	void* get_inbox(){return inbox;}
	zhash_t* get_peers(){return peers;}
	zhash_t* get_peer_groups(){return peer_groups;}
	zre_log* get_log(){return log;}

private:
	zctx_t *ctx;                //  CZMQ context
    void *pipe;                 //  Pipe back to application
    zre_udp *udp;             //  UDP object
    zre_log *log;             //  Log object
    zre_uuid_t *uuid;           //  Our UUID as object
    char *identity;             //  Our UUID as hex string
    void *inbox;                //  Our inbox socket (ROUTER)
    char *host;                 //  Our host IP address
    int port;                   //  Our inbox port number
    char endpoint [30];         //  ipaddress:port endpoint
    byte status;                //  Our own change counter
    zhash_t *peers;             //  Hash of known peers, fast lookup
    zhash_t *peer_groups;       //  Groups that our peers are in
    zhash_t *own_groups;        //  Groups that we are in
    zhash_t *headers;           //  Our header values
    
    fmq_server_t *fmq_server;   //  FileMQ server object
    int fmq_service;            //  FileMQ server port
    char fmq_outbox [255];      //  FileMQ server outbox
    
    fmq_client_t *fmq_client;   //  FileMQ client object
    char fmq_inbox [255];       //  FileMQ client inbox
};

agent::agent(zctx_t *agentContext, void *agentPipe)
{
    void *agentInbox = zsocket_new (agentContext, ZMQ_ROUTER);
    if (!inbox)                 //  Interrupted
        return;

    ctx = agentContext;
    pipe = agentPipe;
    udp = new zre_udp(ZRE_DISCOVERY_PORT);
    inbox = agentInbox;
	host = udp->host();
    port = zsocket_bind (inbox, "tcp://*:*");
    sprintf (endpoint, "%s:%d", host, port);
    if (port < 0) {       //  Interrupted
        delete udp;
        return;
    }
    uuid = zre_uuid_new ();
    identity = _strdup (zre_uuid_str (uuid));
    peers = zhash_new ();
    peer_groups = zhash_new ();
    own_groups = zhash_new ();
    headers = zhash_new ();
    zhash_autofree (headers);
    log = new zre_log (endpoint);

    //  Set up content distribution network: Each server binds to an
    //  ephemeral port and publishes a temporary directory that acts
    //  as the outbox for this node.
    //
    sprintf (fmq_outbox, "%s/send/%s", s_tmpdir (), identity);
    zfile_mkdir (fmq_outbox);
    sprintf (fmq_inbox, "%s/recv/%s", s_tmpdir (), identity);
    zfile_mkdir (fmq_inbox);

    fmq_server = fmq_server_new ();
    fmq_service = fmq_server_bind (fmq_server, "tcp://*:*");
    fmq_server_publish (fmq_server, fmq_outbox, "/");
    fmq_server_set_anonymous (fmq_server, true);
    char publisher [32];
    sprintf (publisher, "tcp://%s:%d", host, fmq_service);
    zhash_update (headers, "X-FILEMQ", publisher);

    //  Client will connect as it discovers new nodes
    fmq_client = fmq_client_new ();
    fmq_client_set_inbox (fmq_client, fmq_inbox);
    fmq_client_set_resync (fmq_client, true);
    fmq_client_subscribe (fmq_client, "/");
}

agent::~agent ()
{
    fmq_dir_t *inbox = fmq_dir_new (fmq_inbox, nullptr);
	if (inbox != nullptr)
    {
		fmq_dir_remove (inbox, true);
		fmq_dir_destroy (&inbox);
	}

    fmq_dir_t *outbox = fmq_dir_new (fmq_outbox, nullptr);
	if (outbox != nullptr)
    {
		fmq_dir_remove (outbox, true);
		fmq_dir_destroy (&outbox);
	}

    zhash_destroy (&peers);
    zhash_destroy (&peer_groups);
    zhash_destroy (&own_groups);
    zhash_destroy (&headers);
    delete udp;
    delete log;
        
    fmq_server_destroy (&fmq_server);
    fmq_client_destroy (&fmq_client);
    free (identity);
}


//  Send message to all peers

static int
s_peer_send (const char *key, void *item, void *argument)
{
    zre_peer *peer = (zre_peer *) item;
    zre_msg_t *msg = zre_msg_dup ((zre_msg_t *) argument);
    peer->send(&msg);
    return 0;
}


//  Here we handle the different control messages from the front-end

int
agent::recv_from_api ()
{
    //  Get the whole message off the pipe in one go
    zmsg_t *request = zmsg_recv (pipe);
    char *command = zmsg_popstr (request);
    if (!command)
        return -1;                  //  Interrupted

    if (streq (command, "WHISPER")) {
        //  Get peer to send message to
        char *identity = zmsg_popstr (request);
        zre_peer *peer = (zre_peer *) zhash_lookup (peers, identity);
        
        //  Send frame on out to peer's mailbox, drop message
        //  if peer doesn't exist (may have been destroyed)
        if (peer) {
            zre_msg_t *msg = zre_msg_new (ZRE_MSG_WHISPER);
            zre_msg_content_set (msg, zmsg_pop (request));
			peer->send(&msg);
        }
        free (identity);
    }
    else
    if (streq (command, "SHOUT")) {
        //  Get group to send message to
        char *name = zmsg_popstr (request);
        zre_group *group = (zre_group *) zhash_lookup (peer_groups, name);
        if (group) {
            zre_msg_t *msg = zre_msg_new (ZRE_MSG_SHOUT);
            zre_msg_group_set (msg, name);
            zre_msg_content_set (msg, zmsg_pop (request));
            group->send(&msg);
        }
        free (name);
    }
    else
    if (streq (command, "JOIN")) {
        char *name = zmsg_popstr (request);
        zre_group *group = (zre_group *) zhash_lookup (own_groups, name);
        if (!group) {
            //  Only send if we're not already in group
            group = new zre_group (name, own_groups);
            zre_msg_t *msg = zre_msg_new (ZRE_MSG_JOIN);
            zre_msg_group_set (msg, name);
            //  Update status before sending command
            zre_msg_status_set (msg, ++(status));
            zhash_foreach (peers, s_peer_send, msg);
            zre_msg_destroy (&msg);
			log->info(ZRE_LOG_MSG_EVENT_JOIN, nullptr, name);
        }
        free (name);
    }
    else
    if (streq (command, "LEAVE")) {
        char *name = zmsg_popstr (request);
        zre_group *group = (zre_group *) zhash_lookup (own_groups, name);
        if (group) {
            //  Only send if we are actually in group
            zre_msg_t *msg = zre_msg_new (ZRE_MSG_LEAVE);
            zre_msg_group_set (msg, name);
            //  Update status before sending command
            zre_msg_status_set (msg, ++(status));
            zhash_foreach (peers, s_peer_send, msg);
            zre_msg_destroy (&msg);
            zhash_delete (own_groups, name);
			log->info(ZRE_LOG_MSG_EVENT_LEAVE, nullptr, name);
        }
        free (name);
    }
    else
    if (streq (command, "SET")) {
        char *name = zmsg_popstr (request);
        char *value = zmsg_popstr (request);
        zhash_update (headers, name, value);
        free (name);
        free (value);
    }
    else
    if (streq (command, "PUBLISH")) {
        char *logical = zmsg_popstr (request);
        char *physical = zmsg_popstr (request);
        //  Virtual filename must start with slash
        assert (logical [0] == '/');
        //  We create symbolic link pointing to real file
        char *symlink = (char*)malloc (strlen (logical) + 3);
        sprintf (symlink, "%s.ln", logical + 1);
        fmq_file_t *file = fmq_file_new (fmq_outbox, symlink);
        int rc = fmq_file_output (file);
        assert (rc == 0);
        fprintf (fmq_file_handle (file), "%s\n", physical);
        fmq_file_destroy (&file);
        free (symlink);
        free (logical);
        free (physical);
    }
    else
    if (streq (command, "RETRACT")) {
        char *logical = zmsg_popstr (request);
        //  Logical filename must start with slash
        assert (logical [0] == '/');
        //  We create symbolic link pointing to real file
        char *symlink = (char*)malloc (strlen (logical) + 3);
        sprintf (symlink, "%s.ln", logical + 1);
        fmq_file_t *file = fmq_file_new (fmq_outbox, symlink);
        fmq_file_remove (file);
        free (symlink);
        free (logical);
    }
    free (command);
    zmsg_destroy (&request);
    return 0;
}

//  Delete peer for a given endpoint

static int
agent_peer_purge (const char *key, void *item, void *argument)
{
    zre_peer *peer = (zre_peer *) item;
    char *endpoint = (char *) argument;
    if (streq (peer->endpoint(), endpoint))
        peer->disconnect();
    return 0;
}

//  Find or create peer via its UUID string

zre_peer *
agent::s_require_peer (char *identity, char *address, uint16_t port)
{
    zre_peer *peer = (zre_peer *) zhash_lookup (peers, identity);
    if (!peer) {
        //  Purge any previous peer on same endpoint
        char endpoint [100];
        snprintf (endpoint, 100, "%s:%hu", address, port);
        zhash_foreach (peers, agent_peer_purge, endpoint);

        peer = new zre_peer(identity, peers, ctx);
        peer->connect(identity, endpoint);

        //  Handshake discovery by sending HELLO as first message
        zre_msg_t *msg = zre_msg_new (ZRE_MSG_HELLO);
        zre_msg_ipaddress_set (msg, udp->host());
        zre_msg_mailbox_set (msg, port);
        zre_msg_groups_set (msg, zhash_keys (own_groups));
        zre_msg_status_set (msg, status);
        zre_msg_headers_set (msg, zhash_dup (headers));
		peer->send(&msg);
        
		log->info(ZRE_LOG_MSG_EVENT_ENTER,
			peer->endpoint(), endpoint);

        //  Now tell the caller about the peer
        zstr_sendm (pipe, "ENTER");
        zstr_send (pipe, identity);
    }
    return peer;
}


//  Find or create group via its name

zre_group *
agent::s_require_peer_group (char *name)
{
    zre_group *group = (zre_group *) zhash_lookup (peer_groups, name);
    if (!group)
        group = new zre_group (name, peer_groups);
    return group;
}

zre_group *
agent::s_join_peer_group (zre_peer *peer, char *name)
{
    zre_group *group = s_require_peer_group (name);
	group->join(peer);

    //  Now tell the caller about the peer joined group
    zstr_sendm (pipe, "JOIN");
	zstr_sendm (pipe, peer->identity());
    zstr_send (pipe, name);

    return group;
}

zre_group *
agent::s_leave_peer_group (zre_peer *peer, char *name)
{
    zre_group *group = s_require_peer_group (name);
	group->leave(peer);

    //  Now tell the caller about the peer left group
    zstr_sendm (pipe, "LEAVE");
	zstr_sendm (pipe, peer->identity());
    zstr_send (pipe, name);

    return group;
}

//  Here we handle messages coming from other peers

int
agent::recv_from_peer ()
{
    //  Router socket tells us the identity of this peer
    zre_msg_t *msg = zre_msg_recv (inbox);
    if (msg == nullptr)
        return 0;               //  Interrupted

    char *identity = zframe_strdup (zre_msg_address (msg));
        
    //  On HELLO we may create the peer if it's unknown
    //  On other commands the peer must already exist
    zre_peer *peer = (zre_peer *) zhash_lookup (peers, identity);
    if (zre_msg_id (msg) == ZRE_MSG_HELLO) {
        peer = s_require_peer (
            identity, zre_msg_ipaddress (msg), zre_msg_mailbox (msg));
        assert (peer);
        peer->ready_set(true);
    }
    //  Ignore command if peer isn't ready
	if (peer == nullptr || !peer->ready()) {
				free(identity);
        zre_msg_destroy (&msg);
        return 0;
    }
    if (!peer->check_message(msg)) {
        zclock_log ("W: [%s] lost messages from %s", identity, identity);
    }

    //  Now process each command
    if (zre_msg_id (msg) == ZRE_MSG_HELLO) {
        //  Join peer to listed groups
        char *name = zre_msg_groups_first (msg);
        while (name) {
            s_join_peer_group (peer, name);
            name = zre_msg_groups_next (msg);
        }
        //  Hello command holds latest status of peer
        peer->status_set(zre_msg_status (msg));
        
        //  Store peer headers for future reference
        peer->headers_set(zre_msg_headers (msg));

        //  If peer is a ZRE/LOG collector, connect to it
        char *collector = zre_msg_headers_string (msg, "X-ZRELOG", nullptr);
        if (collector)
			log->connect(collector);
        
        //  If peer is a FileMQ publisher, connect to it
        char *publisher = zre_msg_headers_string (msg, "X-FILEMQ", nullptr);
        if (publisher)
            fmq_client_connect (fmq_client, publisher);
    }
    else
    if (zre_msg_id (msg) == ZRE_MSG_WHISPER) {
        //  Pass up to caller API as WHISPER event
        zframe_t *cookie = zre_msg_content (msg);
        zstr_sendm (pipe, "WHISPER");
        zstr_sendm (pipe, identity);
        zframe_send (&cookie, pipe, ZFRAME_REUSE); // let msg free the frame
    }
    else
    if (zre_msg_id (msg) == ZRE_MSG_SHOUT) {
        //  Pass up to caller as SHOUT event
        zframe_t *cookie = zre_msg_content (msg);
        zstr_sendm (pipe, "SHOUT");
        zstr_sendm (pipe, identity);
        zstr_sendm (pipe, zre_msg_group (msg));
        zframe_send (&cookie, pipe, ZFRAME_REUSE); // let msg free the frame
    }
    else
    if (zre_msg_id (msg) == ZRE_MSG_PING) {
        zre_msg_t *msg = zre_msg_new (ZRE_MSG_PING_OK);
		peer->send(&msg);
    }
    else
    if (zre_msg_id (msg) == ZRE_MSG_JOIN) {
        s_join_peer_group (peer, zre_msg_group (msg));
        assert (zre_msg_status (msg) == peer->status());
    }
    else
    if (zre_msg_id (msg) == ZRE_MSG_LEAVE) {
        s_leave_peer_group (peer, zre_msg_group (msg));
        assert (zre_msg_status (msg) == peer->status());
    }
    free (identity);
    zre_msg_destroy (&msg);
    
    //  Activity from peer resets peer timers
	peer->refresh();
    return 0;
}

//  Send moar beacon

void
agent::beacon_send ()
{
    //  Beacon object
    beacon_t beacon;
    assert (sizeof (beacon) == 22);

    //  Format beacon fields
    beacon.protocol [0] = 'Z';
    beacon.protocol [1] = 'R';
    beacon.protocol [2] = 'E';
    beacon.version = BEACON_VERSION;
    beacon.port = htons (port);
    zre_uuid_cpy (uuid, beacon.uuid);
    
    //  Broadcast the beacon to anyone who is listening
    udp->send((byte *) &beacon, sizeof (beacon_t));
}


//  Handle beacon

int
agent::recv_udp_beacon ()
{
    //  Get beacon frame from network
    beacon_t beacon;
    ssize_t size = udp->recv((byte *) &beacon, sizeof (beacon_t));

    //  Basic validation on the frame
    if (size != sizeof (beacon_t)
    ||  beacon.protocol [0] != 'Z'
    ||  beacon.protocol [1] != 'R'
    ||  beacon.protocol [2] != 'E'
    ||  beacon.version != BEACON_VERSION)
        return 0;               //  Ignore invalid beacons

    //  If we got a UUID and it's not our own beacon, we have a peer
    if (zre_uuid_neq (uuid, beacon.uuid)) {
        zre_uuid_t *uuid = zre_uuid_new ();
        zre_uuid_set (uuid, beacon.uuid);
        zre_peer *peer = s_require_peer (
            zre_uuid_str (uuid), udp->from(), ntohs (beacon.port));
		peer->refresh();
        zre_uuid_destroy (&uuid);
    }
    return 0;
}


//  Forward event from fmq_client to caller

int
agent::recv_fmq_event ()
{
    zmsg_t *msg = fmq_client_recv (fmq_client);
    if (!msg)
        return 0;
    zmsg_send (&msg, pipe);
    return 0;
}


//  Remove peer from group, if it's a member

static int
agent_peer_delete (const char *key, void *item, void *argument)
{
    zre_group *group = (zre_group *) item;
    zre_peer *peer = (zre_peer *) argument;
	group->leave(peer);
    return 0;
}

//  We do this once a second:
//  - if peer has gone quiet, send TCP ping
//  - if peer has disappeared, expire it

int
agent::ping_peer (const char *key, void *item)
{
    zre_peer *peer = (zre_peer *) item;
	char *identity = peer->identity();
	if (zclock_time () >= peer->expired_at()) {
		get_log()->info(ZRE_LOG_MSG_EVENT_EXIT,
			peer->endpoint(),
			peer->endpoint());
        //  If peer has really vanished, expire it
        zstr_sendm (get_pipe(), "EXIT");
        zstr_send (get_pipe(), identity);
        zhash_foreach (get_peer_groups(), agent_peer_delete, peer);
        zhash_delete (get_peers(), identity);
    }
    else
		if (zclock_time () >= peer->evasive_at()) {
        //  If peer is being evasive, force a TCP ping.
        //  TODO: do this only once for a peer in this state;
        //  it would be nicer to use a proper state machine
        //  for peer management.
        zre_msg_t *msg = zre_msg_new (ZRE_MSG_PING);
		peer->send(&msg);
    }
    return 0;
}

//  The agent handles API commands

static void
zre_node_agent (void *args, zctx_t *ctx, void *pipe)
{
    //  Create agent instance to pass around
	auto self = std::make_shared<agent>(ctx, pipe);
    if (!self)                  //  Interrupted
        return;
    
    //  Send first beacon immediately
    uint64_t ping_at = zclock_time ();
    zmq_pollitem_t pollitems [] = {
        { self->get_pipe(),                           0, ZMQ_POLLIN, 0 },
        { self->get_inbox(),                          0, ZMQ_POLLIN, 0 },
        { 0,           self->get_udp()->handle(), ZMQ_POLLIN, 0 },
        { fmq_client_handle (self->get_fmq_client()), 0, ZMQ_POLLIN, 0 }
    };
    while (!zctx_interrupted()) {
        long timeout = (long) (ping_at - zclock_time ());
        assert (timeout <= PING_INTERVAL);
        if (timeout < 0)
            timeout = 0;
        if (zmq_poll (pollitems, 4, timeout * ZMQ_POLL_MSEC) == -1)
            break;              //  Interrupted

        if (pollitems [0].revents & ZMQ_POLLIN)
            self->recv_from_api ();
        
        if (pollitems [1].revents & ZMQ_POLLIN)
            self->recv_from_peer ();

        if (pollitems [2].revents & ZMQ_POLLIN)
            self->recv_udp_beacon ();

        if (pollitems [3].revents & ZMQ_POLLIN)
            self->recv_fmq_event ();

        if (zclock_time () >= ping_at) {
            self->beacon_send ();
            ping_at = zclock_time () + PING_INTERVAL;
            //  Ping all peers and reap any expired ones
			zhash_foreach (self->get_peers(), [](const char *key, void *item, void* self)
			{
				return ((agent*)self)->ping_peer(key, item);
			}, self.get());
        }

    }
}
