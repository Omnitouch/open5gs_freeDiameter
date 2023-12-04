#include <freeDiameter/extension.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>

#define MODULE_NAME "rt_pref"

/* Static objects */
static struct fd_rt_out_hdl *rt_pref_requests_handle = NULL;
static pthread_rwlock_t rt_pref_lock;

/* Static function prototypes */
static int rt_pref_entry(char *conffile);
static int rt_pref(void * cbdata, struct msg ** msg, struct fd_list * candidates);
bool peerSupportsAppId(DiamId_t diamid, size_t diamidlen, application_id_t id);
bool peerSupportsRelay(DiamId_t diamid, size_t diamidlen);

/* Define the entry point function */
EXTENSION_ENTRY("rt_pref", rt_pref_entry);

/* Cleanup the callbacks */
void fd_ext_fini(void)
{
	TRACE_ENTRY();
	
	/* Unregister the cb */
	fd_rt_out_unregister(rt_pref_requests_handle, NULL);
	
	pthread_rwlock_destroy(&rt_pref_lock);

	/* Done */
	return;
}

/* Entry point called when loading the module */
static int rt_pref_entry(char *conffile) {
	TRACE_ENTRY("%p", conffile);
	int ret = 0;

	pthread_rwlock_init(&rt_pref_lock, NULL);
	if (pthread_rwlock_wrlock(&rt_pref_lock) != 0)
	{
		fd_log_notice("%s: write-lock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	if (pthread_rwlock_unlock(&rt_pref_lock) != 0)
	{
		fd_log_notice("%s: write-unlock failed, aborting", MODULE_NAME);
		return EDEADLK;
	}

	if (0 != (ret = fd_rt_out_register(rt_pref, NULL, 0, &rt_pref_requests_handle)))
	{
		fd_log_error("Cannot register rt_pref_requests callback handler");
		return ret;
	}

	fd_log_notice("Extension 'rt_pref' initialized");
	return 0;
}

static int rt_pref(void * cbdata, struct msg ** msg, struct fd_list * candidates) {
	struct fd_list * li;
	struct msg_hdr *pdata = NULL;

	if (0 != fd_msg_hdr(*msg, &pdata)) {
		LOG_E("Failed to get message header data from message");
		return -1;
	}

	if (0 == pdata->msg_peer_pref) {
		/* msg_peer_pref was unset, do nothing */
		return 0;
	}

	if (pthread_rwlock_wrlock(&rt_pref_lock) != 0)
	{
		fd_log_error("%s: locking failed, aborting message", MODULE_NAME);
		return errno;
	}

	for (li = candidates->next; li != candidates; li = li->next) {
		struct rtd_candidate * cand = (struct rtd_candidate *)li;

		/* If we find a matching peer, we add FD_SCORE_FINALDEST to the 
		 * score to ensure it has preference over the other ones */
		if ((OGS_DIAM_PEER_PREF_RELAY == pdata->msg_peer_pref) &&
			(peerSupportsRelay(cand->diamid, cand->diamidlen))) {
			cand->score += FD_SCORE_FINALDEST;
		} else if (peerSupportsAppId(cand->diamid, cand->diamidlen, pdata->msg_peer_pref)) {
			cand->score += FD_SCORE_FINALDEST;
		}
	}

	if (pthread_rwlock_unlock(&rt_pref_lock) != 0)
	{
		fd_log_error("%s: unlocking failed, returning error", MODULE_NAME);
		return errno;
	}

	return 0;
}

bool peerSupportsAppId(DiamId_t diamid, size_t diamidlen, application_id_t id) {
	bool res = false;
	struct fd_list * li = NULL;
	struct peer_hdr* p = NULL;

	if (0 != fd_peer_getbyid(diamid, diamidlen, 0, &p)) {
		LOG_E("An error occured when searching for peer with diamid '%s'", diamid);
		return false;
	}

	if (NULL == p) {
		/* Couldn't find a peer with this id */
		return false;
	}

	/* For each supported app in peer */
	for (li = &p->info.runtime.pir_apps; li->next != &p->info.runtime.pir_apps; li = li->next) {
		struct fd_app* peer_app_item = (struct fd_app*)(li->next);

		if (id == peer_app_item->appid) {
			res = true;
			break;
		}
	}

	return res;
}

bool peerSupportsRelay(DiamId_t diamid, size_t diamidlen) {
	bool res = false;
	struct peer_hdr* p = NULL;

	if (0 != fd_peer_getbyid(diamid, diamidlen, 0, &p)) {
		LOG_E("An error occured when searching for peer with diamid '%s'", diamid);
		return false;
	}

	if (NULL == p) {
		/* Couldn't find a peer with this id */
		return false;
	}

	if (1 == p->info.runtime.pir_relay) {
		res = true;
	}

	return res;
}