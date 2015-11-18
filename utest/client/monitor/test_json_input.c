#include "../../test.h"
#include "../../builders/build.h"
#include "../../../src/action.h"
#include "../../../src/alloc.h"
#include "../../../src/asfd.h"
#include "../../../src/bu.h"
#include "../../../src/cstat.h"
#include "../../../src/iobuf.h"
#include "../../../src/client/monitor/json_input.h"
#include "../../../src/client/monitor/sel.h"
#include "../../../src/client/monitor/status_client_ncurses.h"
#include "../../builders/build_asfd_mock.h"

#define SRC_DIR	"json_output"

START_TEST(test_json_error)
{
	struct sel *sel;
        struct asfd *asfd;
	fail_unless((asfd=asfd_alloc())!=NULL);
        fail_unless((asfd->rbuf=iobuf_alloc())!=NULL);
	fail_unless((sel=sel_alloc())!=NULL);
	fail_unless(json_input(asfd, sel)==-1);
	json_input_free();
	sel_free(&sel);
	asfd_free(&asfd);
	alloc_check();
}
END_TEST

#define CHUNK_SIZE	10

static void do_read_in_file(const char *path, struct sel *sel)
{
	int lastret=-1;
	struct fzp *fzp;
        struct asfd *asfd;
	char buf[CHUNK_SIZE+1];

	fail_unless((asfd=asfd_alloc())!=NULL);
        fail_unless((asfd->rbuf=iobuf_alloc())!=NULL);
	asfd->rbuf->buf=buf;
	fail_unless((fzp=fzp_open(path, "rb"))!=NULL);
	while(1)
	{
		if((asfd->rbuf->len=fzp_read(fzp,
			asfd->rbuf->buf, CHUNK_SIZE))<=0)
				break;
		asfd->rbuf->buf[asfd->rbuf->len]='\0';
		switch((lastret=json_input(asfd, sel)))
		{
			case 0: continue;
			case 1: break;
			default: break;
		}
	}
	fail_unless(lastret==1);
	fzp_close(&fzp);
	asfd->rbuf->buf=NULL;
	asfd_free(&asfd);
}

static struct sel *read_in_file(const char *path, int times)
{
	int i;
	struct sel *sel;
	fail_unless((sel=sel_alloc())!=NULL);
	for(i=0; i<times; i++)
		do_read_in_file(path, sel);
	json_input_free();
	return sel;
}

static void tear_down(struct sel **sel)
{
	sel_free(sel);
	alloc_check();
}

static void do_test_json_warning(int times)
{
	struct sel *sel;
	fail_unless((sel=read_in_file(SRC_DIR "/warning", times))!=NULL);
	tear_down(&sel);
}

START_TEST(test_json_warning)
{
	do_test_json_warning(1);
	do_test_json_warning(4);
}
END_TEST

static void do_test_json_empty(int times)
{
	struct sel *sel;
	fail_unless((sel=read_in_file(SRC_DIR "/empty", times))!=NULL);
	fail_unless(sel->clist==NULL);
	tear_down(&sel);
}

START_TEST(test_json_empty)
{
	do_test_json_empty(1);
	do_test_json_empty(4);
}
END_TEST

static void do_test_json_clients(int times)
{
	struct sel *sel;
	const char *cnames[] ={"cli1", "cli2", "cli3", NULL};
	fail_unless((sel=read_in_file(SRC_DIR "/clients", times))!=NULL);
	fail_unless(sel->clist!=NULL);
	assert_cstat_list(sel->clist, cnames);
	tear_down(&sel);
}

START_TEST(test_json_clients)
{
	do_test_json_clients(1);
	do_test_json_clients(4);
}
END_TEST

static struct sd sd1[] = {
	{ "0000001 1970-01-01 00:00:00", 1, 1, BU_DELETABLE|BU_CURRENT },
};

// FIX THIS - this should only check the most recent backup in the list.
// This should come out in the wash when I do clients with multiple backups.
static void assert_bu_list_minimal(struct bu *bu, struct sd *s)
{
	fail_unless(bu!=NULL);
	fail_unless(s->bno==bu->bno);
	fail_unless(s->flags==bu->flags);
// FIX THIS
//printf("%d %d\n", s->timestamp, bu->timestamp);
//	fail_unless(s->timestamp==bu->timestamp);
//if(bu->next)
//printf("bu next\n");
}

static void do_test_json_clients_with_backup(const char *path,
	struct sd *sd, int s, int times)
{
	struct cstat *c;
	struct sel *sel;
	const char *cnames[] ={"cli1", "cli2", "cli3", NULL};
	fail_unless((sel=read_in_file(path, times))!=NULL);
	fail_unless(sel->clist!=NULL);
	assert_cstat_list(sel->clist, cnames);
	for(c=sel->clist; c; c=c->next)
		assert_bu_list_minimal(c->bu, &sd[s-1]);
	tear_down(&sel);
}

START_TEST(test_json_clients_with_backup)
{
	int s=ARR_LEN(sd1);
	const char *path=SRC_DIR "/clients_with_backup";
	do_test_json_clients_with_backup(path, sd1, s, 1);
	do_test_json_clients_with_backup(path, sd1, s, 4);
}
END_TEST

static struct sd sd12345[] = {
	{ "0000001 1970-01-01 00:00:00", 1, 1, BU_DELETABLE|BU_MANIFEST },
	{ "0000002 1970-01-02 00:00:00", 2, 2, 0 },
	{ "0000003 1970-01-03 00:00:00", 3, 3, BU_HARDLINKED },
	{ "0000004 1970-01-04 00:00:00", 4, 4, 0 },
	{ "0000005 1970-01-05 00:00:00", 5, 5, BU_CURRENT|BU_MANIFEST}
};

START_TEST(test_json_clients_with_backups)
{
	int s=ARR_LEN(sd12345);
	const char *path=SRC_DIR "/clients_with_backups";
	do_test_json_clients_with_backup(path, sd12345, s, 1);
	do_test_json_clients_with_backup(path, sd12345, s, 4);
}
END_TEST

static struct sd sd123w[] = {
	{ "0000001 1970-01-01 00:00:00", 1, 1, BU_DELETABLE|BU_MANIFEST },
	{ "0000002 1970-01-02 00:00:00", 2, 2, BU_CURRENT|BU_MANIFEST },
	{ "0000003 1970-01-03 00:00:00", 3, 3, BU_WORKING },
};

START_TEST(test_json_clients_with_backups_working)
{
	int s=ARR_LEN(sd123w);
	const char *path=SRC_DIR "/clients_with_backups_working";
	do_test_json_clients_with_backup(path, sd123w, s, 1);
	do_test_json_clients_with_backup(path, sd123w, s, 4);
}
END_TEST

Suite *suite_client_monitor_json_input(void)
{
	Suite *s;
	TCase *tc_core;

	s=suite_create("client_monitor_json_input");

	tc_core=tcase_create("Core");

	tcase_add_test(tc_core, test_json_error);
	tcase_add_test(tc_core, test_json_warning);
	tcase_add_test(tc_core, test_json_empty);
	tcase_add_test(tc_core, test_json_clients);
	tcase_add_test(tc_core, test_json_clients_with_backup);
	tcase_add_test(tc_core, test_json_clients_with_backups);
	tcase_add_test(tc_core, test_json_clients_with_backups_working);
	suite_add_tcase(s, tc_core);

	return s;
}
