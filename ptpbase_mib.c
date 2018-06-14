/**
 * @file ptpbase_mib.c
 * @brief Implements PTPv2 Management Information Base (RFC 8173)
 * @note Copyright (C) 2018 Anders Selhammer <anders.selhammer@est.tech>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>
#include <net-snmp/agent/net-snmp-agent-includes.h>

#include "clock.h"
#include "ds.h"
#include "pdt.h"
#include "print.h"
#include "snmpd_mib.h"
#include "tlv.h"
#include "tmv.h"

/*
 * ptp base oid values
 */
#define SNMP_OID_PTPBASE_MIB                  1, 3, 6, 1, 2, 1, 241
#define SNMP_OID_PTPBASE_OBJ_CLOCKINFO        SNMP_OID_PTPBASE_MIB, 1, 2
#define SNMP_OID_PTPBASE_CLOCK_CURRENT_DS     SNMP_OID_PTPBASE_OBJ_CLOCKINFO, 1
#define SNMP_OID_PTPBASE_CLOCK_PARENT_DS      SNMP_OID_PTPBASE_OBJ_CLOCKINFO, 2
#define SNMP_OID_PTPBASE_CLOCK_DEFAULT_DS     SNMP_OID_PTPBASE_OBJ_CLOCKINFO, 3
#define SNMP_OID_PTPBASE_CLOCK_TIMEPROP_DS    SNMP_OID_PTPBASE_OBJ_CLOCKINFO, 5
#define SNMP_OID_PTPBASE_CLOCK_PORT_DS        SNMP_OID_PTPBASE_OBJ_CLOCKINFO, 8

/*
 * column number definitions for tables
 */
#define COLUMN_PTPBASE_DOMAININDEX                              1
#define COLUMN_PTPBASE_CLOCKTYPEINDEX                           2
#define COLUMN_PTPBASE_INSTANCEINDEX                            3
#define COLUMN_PTPBASE_MIN_DS_COLUMN                            4

#define COLUMN_CURRENTDS_STEPSREMOVED                           4
#define COLUMN_CURRENTDS_OFFSETFROMMASTER                       5
#define COLUMN_CURRENTDS_MEANPATHDELAY                          6

#define COLUMN_PARENTDS_PARENTPORTIDENTITY                      4
#define COLUMN_PARENTDS_PARENTSTATS                             5
#define COLUMN_PARENTDS_OFFSET                                  6
#define COLUMN_PARENTDS_CLOCKPHCHRATE                           7
#define COLUMN_PARENTDS_GMCLOCKIDENTITY                         8
#define COLUMN_PARENTDS_GMCLOCKPRIORITY1                        9
#define COLUMN_PARENTDS_GMCLOCKPRIORITY2                        10
#define COLUMN_PARENTDS_GMCLOCKQUALITYCLASS                     11
#define COLUMN_PARENTDS_GMCLOCKQUALITYACCURACY                  12
#define COLUMN_PARENTDS_GMCLOCKQUALITYOFFSET                    13

#define COLUMN_DEFAULTDS_TWOSTEPFLAG                            4
#define COLUMN_DEFAULTDS_CLOCKIDENTITY                          5
#define COLUMN_DEFAULTDS_PRIORITY1                              6
#define COLUMN_DEFAULTDS_PRIORITY2                              7
#define COLUMN_DEFAULTDS_SLAVEONLY                              8
#define COLUMN_DEFAULTDS_QUALITYCLASS                           9
#define COLUMN_DEFAULTDS_QUALITYACCURACY                        10
#define COLUMN_DEFAULTDS_QUALITYOFFSET                          11

#define COLUMN_TIMEPROPERTIESDS_CURRENTUTCOFFSETVALID           4
#define COLUMN_TIMEPROPERTIESDS_CURRENTUTCOFFSET                5
#define COLUMN_TIMEPROPERTIESDS_LEAP59                          6
#define COLUMN_TIMEPROPERTIESDS_LEAP61                          7
#define COLUMN_TIMEPROPERTIESDS_TIMETRACEABLE                   8
#define COLUMN_TIMEPROPERTIESDS_FREQTRACEABLE                   9
#define COLUMN_TIMEPROPERTIESDS_PTPTIMESCALE                    10
#define COLUMN_TIMEPROPERTIESDS_SOURCE                          11

/*
 * other internal defines and types
 */
#define CMD_MAX_LEN 1024

enum PtpClockType {
        ordinaryClock = 1,
        boundaryClock,
        transparentClock,
        boundaryNode
};

enum rettype {
	SNMPD_UNSIGNED = 0,
};

union retval {
	unsigned int ui;
};

struct retdata {
	enum rettype type;
	union retval val;
};

struct ptpbase_table_index {
	u_long domain_index;
	long   clock_type_index;
	u_long instance_index;
	int    valid;
};

struct ptpbase_table_entry {
	struct ptpbase_table_index idxs;
	int                        id;
	unsigned int               tmo;
	struct ptp_message         *msg;
};

struct ptpbase_table_entry *entry_head = NULL;


/*
 * function declarations
 */

static void reset_ds_head(struct ptpbase_table_entry *head)
{
	pr_err("reset_ds_head");
	head->id = 0;
	if (head->tmo) {
		snmp_alarm_unregister(head->tmo);
		head->tmo = 0;
	}
	if (head->msg) {
		msg_put(head->msg);
		head->msg = NULL;
	}
}

static void ds_cb(unsigned int clientreg, void *clientarg)
{
	pr_err("ds_cb %u", clientreg);
	struct ptpbase_table_entry *head =
		(struct ptpbase_table_entry *)clientarg;
	if (clientreg != head->tmo) {
		pr_err("Received outdated timer");
		return;
	}
	head->idxs.valid = 0;
	reset_ds_head(head);
}

static int get_mgmt_data(struct ptp_message *msg,
			 struct management_tlv **mgt)
{
	int action;
	struct TLV *tlv;

	if (msg_type(msg) != MANAGEMENT) {
		pr_err("msg type not MANAGEMENT");
		return -1;
	}

	action = management_action(msg);
	if (action < GET || action > ACKNOWLEDGE) {
		pr_err("incorrect action");
		return -1;
	}

	if (msg->tlv_count != 1) {
		pr_err("incorrect tlv count");
		return -1;
	}

	tlv = (struct TLV *) msg->management.suffix;
	if (tlv->type == TLV_MANAGEMENT) {
		;
	} else if (tlv->type == TLV_MANAGEMENT_ERROR_STATUS) {
		pr_err("MANAGEMENT_ERROR_STATUS");
		return -1;
	} else {
		pr_err("unknown-tlv");
		return -1;
	}

	*mgt = (struct management_tlv *) msg->management.suffix;
	if ((*mgt)->length == 2 && (*mgt)->id != TLV_NULL_MANAGEMENT) {
		pr_err("empty-tlv");
		return -1;
	}
	return 0;
}

static int command_from_id(int tlv, char *cmd)
{
	switch (tlv) {
	case TLV_DEFAULT_DATA_SET:
		snprintf(cmd, CMD_MAX_LEN, "GET DEFAULT_DATA_SET");
		break;
	case TLV_CURRENT_DATA_SET:
		snprintf(cmd, CMD_MAX_LEN, "GET CURRENT_DATA_SET");
		break;
	case TLV_PARENT_DATA_SET:
		snprintf(cmd, CMD_MAX_LEN, "GET PARENT_DATA_SET");
		break;
	case TLV_TIME_PROPERTIES_DATA_SET:
		snprintf(cmd, CMD_MAX_LEN, "GET TIME_PROPERTIES_DATA_SET");
		break;
	default:
		return -1;
	}
	return 0;
}

static int get_msg_retdata(struct ptp_message *msg, struct retdata *ret)
{
	struct management_tlv *mgt = NULL;
	struct management_tlv_datum *mtd;
	struct tlv_extra *extra = NULL;

	if (get_mgmt_data(msg, &mgt)) {
		return -1;
	}
	extra = TAILQ_FIRST(&msg->tlv_list);

	switch (mgt->id) {
	case TLV_CLOCK_DESCRIPTION:
		switch (align16(extra->cd.clockType)) {
		case CLOCK_TYPE_ORDINARY:
			ret->val.ui = ordinaryClock;
			break;
		case CLOCK_TYPE_BOUNDARY:
			ret->val.ui = boundaryClock;
			break;
		case CLOCK_TYPE_P2P:
		case CLOCK_TYPE_E2E:
			ret->val.ui = transparentClock;
			break;
		default:
			return -1;
		}
		ret->type = SNMPD_UNSIGNED;
		break;
	case TLV_PRIORITY1:
	case TLV_PRIORITY2:
	case TLV_VERSION_NUMBER:
		mtd = (struct management_tlv_datum *) mgt->data;
		ret->val.ui = mtd->val;
		ret->type = SNMPD_UNSIGNED;
		break;
	default:
		pr_err("No matching TLV");
		return -1;
	}
	return 0;
}

static int get_msg_ds(struct ptp_message *msg)
{
	struct management_tlv *mgt = NULL;

	if (get_mgmt_data(msg, &mgt)) {
		return -1;
	}
	entry_head->id = mgt->id;
	entry_head->msg = msg;
	return 0;
}

static int update_idxs()
{
	pr_err("update_idxs");
	struct ptp_message *msg;
	struct retdata ret;

	msg = snmpd_run_pmc("GET CLOCK_DESCRIPTION");
	if (!msg) {
		return SNMP_ERR_GENERR;
	}
	if (get_msg_retdata(msg, &ret)) {
		msg_put(msg);
		return SNMP_ERR_GENERR;
	}
	msg_put(msg);
	entry_head->idxs.domain_index = snmpd_get_domain();
	entry_head->idxs.clock_type_index = ret.val.ui;
	entry_head->idxs.instance_index = 99;
	entry_head->idxs.valid = 1;
	return SNMP_ERR_NOERROR;
}

static int update_entry(char *cmd)
{
	pr_err("update_entry");
	struct ptp_message *msg;

	msg = snmpd_run_pmc(cmd);
	if (!msg) {
		return SNMP_ERR_GENERR;
	}
	if (get_msg_ds(msg)) {
		msg_put(msg);
		return SNMP_ERR_GENERR;
	}
	return SNMP_ERR_NOERROR;
}

static netsnmp_variable_list *get_next_data_point(void **my_loop_context,
						  void **my_data_context,
						  netsnmp_variable_list
						  *put_index_data,
						  netsnmp_iterator_info
						  *mydata)
{
	pr_err("get_next_data_point %u", *(int*)mydata->myvoid);
	struct ptpbase_table_entry *entry =
		(struct ptpbase_table_entry *) *my_loop_context;
	netsnmp_variable_list *idx = put_index_data;

	if (entry) {
		snmp_set_var_typed_integer(idx, ASN_UNSIGNED,
					   entry->idxs.domain_index);
		idx = idx->next_variable;
		snmp_set_var_typed_integer(idx, ASN_INTEGER,
					   entry->idxs.clock_type_index);
		idx = idx->next_variable;
		snmp_set_var_typed_integer(idx, ASN_UNSIGNED,
					   entry->idxs.instance_index);
		*my_data_context = (void *) entry;
		*my_loop_context = (void *) NULL;
		return put_index_data;
	} else {
		return NULL;
	}
}

static netsnmp_variable_list *get_first_data_point(void **my_loop_context,
						   void **my_data_context,
						   netsnmp_variable_list
						   *put_index_data,
						   netsnmp_iterator_info
						   *mydata)
{
	pr_err("get_first_data_point %u", *(int*)mydata->myvoid);
	char cmd[CMD_MAX_LEN];
	if (!entry_head->idxs.valid) {
		if (update_idxs()) {
			return NULL;
		}
	}
	if (entry_head->id != *(int*)mydata->myvoid) {
		reset_ds_head(entry_head);
		entry_head->tmo = snmp_alarm_register(1, 0, ds_cb, entry_head);
		if (command_from_id(*(int*)mydata->myvoid, cmd)) {
			return NULL;
		}
		if (update_entry(cmd)) {
			return NULL;
		}
	}
	*my_loop_context = entry_head;
	return get_next_data_point(my_loop_context, my_data_context,
                  		   put_index_data, mydata);
}

static int set_cds_return_values(netsnmp_table_request_info *table_info,
				 struct ptpbase_table_entry *table_entry,
				 netsnmp_request_info *request)
{
	struct management_tlv *mgt;
	struct currentDS *ds;
	mgt = (struct management_tlv *) table_entry->msg->management.suffix;
	ds = (struct currentDS *) mgt->data;
	Octet str[255];
	switch (table_info->colnum) {
	case COLUMN_CURRENTDS_STEPSREMOVED:
		pr_err("COLUMN_CURRENTDS_STEPSREMOVED");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_UNSIGNED,
					   ds->stepsRemoved);
		break;
	case COLUMN_CURRENTDS_OFFSETFROMMASTER:
		pr_err("COLUMN_CURRENTDS_OFFSETFROMMASTER %lu",
			sizeof(ds->offsetFromMaster));
		memcpy(str, &ds->offsetFromMaster, sizeof(ds->offsetFromMaster));
		snmp_set_var_typed_value(request->requestvb,
					 ASN_OCTET_STR,
					 str, sizeof(ds->offsetFromMaster));
		break;
	case COLUMN_CURRENTDS_MEANPATHDELAY:
		pr_err("COLUMN_CURRENTDS_MEANPATHDELAY");
		memcpy(str, &ds->meanPathDelay, sizeof(ds->meanPathDelay));
		snmp_set_var_typed_value(request->requestvb,
					 ASN_OCTET_STR,
				 	 str, sizeof(ds->meanPathDelay));
		break;
	default:
		return -1;
	}
	return 0;
}

static int set_pds_return_values(netsnmp_table_request_info *table_info,
				 struct ptpbase_table_entry *table_entry,
				 netsnmp_request_info *request)
{
	struct management_tlv *mgt;
	struct parentDS *ds;
	mgt = (struct management_tlv *) table_entry->msg->management.suffix;
	ds = (struct parentDS *) mgt->data;
	Octet str[255];
	switch (table_info->colnum) {
	case COLUMN_PARENTDS_PARENTPORTIDENTITY:
		pr_err("COLUMN_PARENTDS_PARENTPORTIDENTITY %s %lu", pid2str(&ds->parentPortIdentity), sizeof(ds->parentPortIdentity));
		memcpy(str, &ds->parentPortIdentity,
		       sizeof(ds->parentPortIdentity));
		snmp_set_var_typed_value(request->requestvb,
					 ASN_OCTET_STR,
					 str, sizeof(ds->parentPortIdentity));
		break;
	case COLUMN_PARENTDS_PARENTSTATS:
		pr_err("COLUMN_PARENTDS_PARENTSTATS");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->parentStats);
		break;
	case COLUMN_PARENTDS_OFFSET:
		pr_err("COLUMN_PARENTDS_OFFSET");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->observedParentOffsetScaledLogVariance);
		break;
	case COLUMN_PARENTDS_CLOCKPHCHRATE:
		pr_err("COLUMN_PARENTDS_CLOCKPHCHRATE");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->observedParentClockPhaseChangeRate);
		break;
	case COLUMN_PARENTDS_GMCLOCKIDENTITY:
		pr_err("COLUMN_PARENTDS_GMCLOCKIDENTITY %s %lu", cid2str(&ds->grandmasterIdentity), sizeof(ds->grandmasterIdentity));
		memcpy(str, &ds->grandmasterIdentity,
		       sizeof(ds->grandmasterIdentity));
		snmp_set_var_typed_value(request->requestvb,
					 ASN_OCTET_STR,
					 str, sizeof(ds->grandmasterIdentity));
		break;
	case COLUMN_PARENTDS_GMCLOCKPRIORITY1:
		pr_err("COLUMN_PARENTDS_GMCLOCKPRIORITY1");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_UNSIGNED,
					   ds->grandmasterPriority1);
		break;
	case COLUMN_PARENTDS_GMCLOCKPRIORITY2:
		pr_err("COLUMN_PARENTDS_GMCLOCKPRIORITY2");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_UNSIGNED,
					   ds->grandmasterPriority2);
		break;
	case COLUMN_PARENTDS_GMCLOCKQUALITYCLASS:
		pr_err("COLUMN_PARENTDS_GMCLOCKQUALITYCLASS");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->grandmasterClockQuality.clockClass);
		break;
	case COLUMN_PARENTDS_GMCLOCKQUALITYACCURACY:
		pr_err("COLUMN_PARENTDS_GMCLOCKQUALITYACCURACY");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->grandmasterClockQuality.clockAccuracy);
		break;
	case COLUMN_PARENTDS_GMCLOCKQUALITYOFFSET:
		pr_err("COLUMN_PARENTDS_GMCLOCKQUALITYOFFSET");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_UNSIGNED,
					   ds->grandmasterClockQuality.offsetScaledLogVariance);
		break;
	default:
		return -1;
	}
	return 0;
}

static int set_dds_return_values(netsnmp_table_request_info *table_info,
		struct ptpbase_table_entry *table_entry,
		netsnmp_request_info *request) {
	struct management_tlv *mgt;
	struct defaultDS *ds;
	Octet str[255];
	mgt = (struct management_tlv *) table_entry->msg->management.suffix;
	ds = (struct defaultDS *) mgt->data;
	switch (table_info->colnum) {
	case COLUMN_DEFAULTDS_TWOSTEPFLAG:
		pr_err("COLUMN_DEFAULTDS_TWOSTEPFLAG");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & DDS_TWO_STEP_FLAG ? 1 : 0);
		break;
	case COLUMN_DEFAULTDS_CLOCKIDENTITY:
		pr_err("COLUMN_DEFAULTDS_CLOCKIDENTITY %s", cid2str(&ds->clockIdentity));
		memcpy(str, &ds->clockIdentity,
		       sizeof(ds->clockIdentity));
		snmp_set_var_typed_value(request->requestvb,
					 ASN_OCTET_STR,
					 str, sizeof(ds->clockIdentity));
		break;
	case COLUMN_DEFAULTDS_PRIORITY1:
		pr_err("COLUMN_DEFAULTDS_PRIORITY1");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_UNSIGNED,
					   ds->priority1);
		break;
	case COLUMN_DEFAULTDS_PRIORITY2:
		pr_err("COLUMN_DEFAULTDS_PRIORITY2");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_UNSIGNED,
					   ds->priority2);
		break;
	case COLUMN_DEFAULTDS_SLAVEONLY:
		pr_err("COLUMN_DEFAULTDS_SLAVEONLY");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & DDS_SLAVE_ONLY ? 1 : 0);
		break;
	case COLUMN_DEFAULTDS_QUALITYCLASS:
		pr_err("COLUMN_DEFAULTDS_QUALITYCLASS");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->clockQuality.
					   clockClass);
		break;
	case COLUMN_DEFAULTDS_QUALITYACCURACY:
		pr_err("COLUMN_DEFAULTDS_QUALITYACCURACY");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->clockQuality.
					   clockAccuracy);
		break;
	case COLUMN_DEFAULTDS_QUALITYOFFSET:
		pr_err("COLUMN_DEFAULTDS_QUALITYOFFSET");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->clockQuality.
					   offsetScaledLogVariance);
		break;
	default:
		return -1;
	}
	return 0;
}

static int set_tds_return_values(netsnmp_table_request_info *table_info,
				 struct ptpbase_table_entry *table_entry,
				 netsnmp_request_info *request)
{
	struct management_tlv *mgt;
	struct timePropertiesDS *ds;
	mgt = (struct management_tlv *) table_entry->msg->management.suffix;
	ds = (struct timePropertiesDS *) mgt->data;
	switch (table_info->colnum) {
	case COLUMN_TIMEPROPERTIESDS_CURRENTUTCOFFSETVALID:
		pr_err("COLUMN_TIMEPROPERTIESDS_CURRENTUTCOFFSETVALID");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & UTC_OFF_VALID ? 1 : 0);
		break;
	case COLUMN_TIMEPROPERTIESDS_CURRENTUTCOFFSET:
		pr_err("COLUMN_TIMEPROPERTIESDS_CURRENTUTCOFFSET");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->currentUtcOffset);
		break;
	case COLUMN_TIMEPROPERTIESDS_LEAP59:
		pr_err("COLUMN_TIMEPROPERTIESDS_LEAP59");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & LEAP_59 ? 1 : 0);
		break;
	case COLUMN_TIMEPROPERTIESDS_LEAP61:
		pr_err("COLUMN_TIMEPROPERTIESDS_LEAP61");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & LEAP_61 ? 1 : 0);
		break;
	case COLUMN_TIMEPROPERTIESDS_TIMETRACEABLE:
		pr_err("COLUMN_TIMEPROPERTIESDS_TIMETRACEABLE");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & TIME_TRACEABLE ? 1 : 0);
		break;
	case COLUMN_TIMEPROPERTIESDS_FREQTRACEABLE:
		pr_err("COLUMN_TIMEPROPERTIESDS_FREQTRACEABLE");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & FREQ_TRACEABLE ? 1 : 0);
		break;
	case COLUMN_TIMEPROPERTIESDS_PTPTIMESCALE:
		pr_err("COLUMN_TIMEPROPERTIESDS_PTPTIMESCALE");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->flags & PTP_TIMESCALE ? 1 : 0);
		break;
	case COLUMN_TIMEPROPERTIESDS_SOURCE:
		pr_err("COLUMN_TIMEPROPERTIESDS_SOURCE");
		snmp_set_var_typed_integer(request->requestvb,
					   ASN_INTEGER,
					   ds->timeSource);
		break;
	default:
		return -1;
	}
	return 0;
}

static int ds_handler(netsnmp_mib_handler *handler,
		      netsnmp_handler_registration *reginfo,
		      netsnmp_agent_request_info *reqinfo,
		      netsnmp_request_info *requests)
{
	pr_err("ds_handler %s", reginfo->handlerName);
	netsnmp_request_info *request;
	netsnmp_table_request_info *table_info;
	struct ptpbase_table_entry *table_entry;

	if (reqinfo->mode != MODE_GET) {
		return SNMP_ERR_NOERROR;
	}

	/*
	 * Read-support (also covers GetNext requests)
	 */
	for (request = requests; request; request = request->next) {
		table_entry = (struct ptpbase_table_entry *)
			netsnmp_extract_iterator_context(request);
		table_info = netsnmp_extract_table_info(request);
		if (!table_info->colnum || !table_entry) {
			netsnmp_set_request_error(reqinfo, request,
						  SNMP_NOSUCHINSTANCE);
			continue;
		}
		switch (table_entry->id) {
		case TLV_CURRENT_DATA_SET:
			if (set_cds_return_values(table_info, table_entry,
						  request)) {
				netsnmp_set_request_error(reqinfo, request,
							  SNMP_NOSUCHOBJECT);
			}
			break;
		case TLV_PARENT_DATA_SET:
			if (set_pds_return_values(table_info, table_entry,
						  request)) {
				netsnmp_set_request_error(reqinfo, request,
							  SNMP_NOSUCHOBJECT);
			}
			break;
		case TLV_DEFAULT_DATA_SET:
			if (set_dds_return_values(table_info, table_entry,
						  request)) {
				netsnmp_set_request_error(reqinfo, request,
							  SNMP_NOSUCHOBJECT);
			}
			break;
		case TLV_TIME_PROPERTIES_DATA_SET:
			if (set_tds_return_values(table_info, table_entry,
						  request)) {
				netsnmp_set_request_error(reqinfo, request,
							  SNMP_NOSUCHOBJECT);
			}
			break;
		}
	}
	return SNMP_ERR_NOERROR;
}

static int init_table_dataset(const char *ds_name,
			      const oid *ds_oid,
			      const size_t ds_oid_len,
			      unsigned int max_column,
			      int *ds_id)
{
	netsnmp_table_registration_info *table_info;
	netsnmp_handler_registration *reg;
	netsnmp_iterator_info *iinfo;

	table_info = SNMP_MALLOC_TYPEDEF(netsnmp_table_registration_info);
	netsnmp_table_helper_add_indexes(table_info,
					 ASN_UNSIGNED,
					 ASN_INTEGER,
					 ASN_UNSIGNED,
					 0);
	table_info->min_column = COLUMN_PTPBASE_MIN_DS_COLUMN;
	table_info->max_column = max_column;

	reg = netsnmp_create_handler_registration(ds_name,
						  ds_handler,
						  ds_oid,
						  ds_oid_len,
						  HANDLER_CAN_RONLY);

	iinfo = SNMP_MALLOC_TYPEDEF(netsnmp_iterator_info);
	iinfo->get_first_data_point = get_first_data_point;
	iinfo->get_next_data_point = get_next_data_point;
	iinfo->table_reginfo = table_info;
	iinfo->flags |= NETSNMP_ITERATOR_FLAG_SORTED;
	iinfo->myvoid = ds_id;

	netsnmp_register_table_iterator(reg, iinfo);
	return 0;
}

int init_ptpbase_mib()
{
	static int current_ds_id    = TLV_CURRENT_DATA_SET;
	static int parent_ds_id     = TLV_PARENT_DATA_SET;
	static int default_ds_id    = TLV_DEFAULT_DATA_SET;
	static int timeprop_ds_id   = TLV_TIME_PROPERTIES_DATA_SET;
	const oid current_ds_oid[]  = { SNMP_OID_PTPBASE_CLOCK_CURRENT_DS };
	const oid parent_ds_oid[]   = { SNMP_OID_PTPBASE_CLOCK_PARENT_DS };
	const oid default_ds_oid[]  = { SNMP_OID_PTPBASE_CLOCK_DEFAULT_DS };
	const oid timeprop_ds_oid[] = { SNMP_OID_PTPBASE_CLOCK_TIMEPROP_DS };
	if (init_table_dataset("clock_current_ds", current_ds_oid,
			       OID_LENGTH(current_ds_oid),
			       COLUMN_CURRENTDS_MEANPATHDELAY,
			       &current_ds_id)) {
		pr_err("Failed to initialize clock_current_ds");
		return -1;
	}
	if (init_table_dataset("clock_parent_ds", parent_ds_oid,
			       OID_LENGTH(parent_ds_oid),
			       COLUMN_PARENTDS_GMCLOCKQUALITYOFFSET,
			       &parent_ds_id)) {
		pr_err("Failed to initialize clock_current_ds");
		return -1;
	}
	if (init_table_dataset("clock_default_ds", default_ds_oid,
			       OID_LENGTH(default_ds_oid),
			       COLUMN_DEFAULTDS_QUALITYOFFSET,
			       &default_ds_id)) {
		pr_err("Failed to initialize clock_current_ds");
		return -1;
	}
	if (init_table_dataset("clock_timeprop_ds", timeprop_ds_oid,
			       OID_LENGTH(timeprop_ds_oid),
			       COLUMN_TIMEPROPERTIESDS_SOURCE,
			       &timeprop_ds_id)) {
		pr_err("Failed to initialize clock_current_ds");
		return -1;
	}

	entry_head = SNMP_MALLOC_TYPEDEF(struct ptpbase_table_entry);
	return (entry_head) ? 0 : -1;
}

void free_ptpbase_mib()
{
	msg_put(entry_head->msg);
	SNMP_FREE(entry_head);
}


//	if (init_current_ds_steps_removed()) {
//		pr_err("Failed to initialize current_ds_steps_removed");
//	}
//	if (init_default_ds_priority1()) {
//		pr_err("Failed to initialize default_ds_priority1");
//	}
//	if (init_default_ds_priority2()) {
//		pr_err("Failed to initialize default_ds_priority2");
//	}
//	if (init_port_ds_ptp_version()) {
//		pr_err("Failed to initialize port_ds_ptp_version");
//	}

//static int command_lookup(char *name, char *cmd)
//{
//	if (strcmp(name, "clock_current_ds") == 0) {
//		snprintf(cmd, CMD_MAX_LEN, "GET CURRENT_DATA_SET");
//	} else if (strcmp(name, "current_ds_steps_removed") == 0) {
//		snprintf(cmd, CMD_MAX_LEN, "GET DOMAIN");
//	} else if (strcmp(name, "default_ds_priority1") == 0) {
//		snprintf(cmd, CMD_MAX_LEN, "GET PRIORITY1");
//	} else if (strcmp(name, "default_ds_priority2") == 0) {
//		snprintf(cmd, CMD_MAX_LEN, "GET PRIORITY2");
//	} else if (strcmp(name, "port_ds_ptp_version") == 0) {
//		snprintf(cmd, CMD_MAX_LEN, "GET VERSION_NUMBER");
//	} else {
//		return -1;
//	}
//	return 0;
//}

//static int handle_pdu(netsnmp_mib_handler *handler,
//		      netsnmp_handler_registration *reginfo,
//		      netsnmp_agent_request_info *reqinfo,
//		      netsnmp_request_info *requests)
//{
//	struct ptp_message *msg;
//	char cmd[CMD_MAX_LEN];
//	struct retdata ret;
//	int err = -1, cmp;
//	cmp = snmp_oid_compare(requests->requestvb->name,
//			requests->requestvb->name_length,
//			reginfo->rootoid, reginfo->rootoid_len);
////	pr_err("cmp %d inclusive %d", cmp, requests->inclusive);
////	pr_err("reginfo->rootoid_len %d", (int)reginfo->rootoid_len);
//	pr_err("reginfo->handlerName %s", reginfo->handlerName);
//
//	if (command_lookup(reginfo->handlerName, cmd)) {
//		pr_err("Failed to lookup command from reginfo");
//		return SNMP_ERR_GENERR;
//	}
//
//	switch (reqinfo->mode) {
//	case MODE_GET:
//		msg = snmpd_run_pmc(cmd);
//		if (!msg) {
//			return SNMP_ERR_GENERR;
//		}
//		if (get_msg_retdata(msg, &ret)) {
//			msg_put(msg);
//			return SNMP_ERR_GENERR;
//		}
//		msg_put(msg);
//		switch (ret.type) {
//		case SNMPD_UNSIGNED:
//			err = snmp_set_var_typed_value(requests->requestvb,
//						       ASN_UNSIGNED,
//						       &ret.val.ui,
//						       sizeof(ret.val));
//			break;
//		}
//		if (err) {
//			return SNMP_ERR_GENERR;
//		}
//		break;
//	default:
//		snmp_log(LOG_ERR, "unknown mode (%d) in handle_totalClients\n",
//			 reqinfo->mode);
//		return SNMP_ERR_GENERR;
//	}
//	return SNMP_ERR_NOERROR;
//}

//static int register_scalar(netsnmp_handler_registration *reginfo)
//{
//	reginfo->rootoid = (oid*)realloc(reginfo->rootoid,
//					 (reginfo->rootoid_len+1) * sizeof(oid));
//	reginfo->rootoid[reginfo->rootoid_len] = 0;
//
//	netsnmp_inject_handler(reginfo, netsnmp_get_instance_handler());
//	netsnmp_inject_handler(reginfo, netsnmp_get_scalar_handler());
//	return netsnmp_register_serialize(reginfo);
//}
//
//
//
//static int init_default_ds_priority1(void)
//{
//	netsnmp_handler_registration *hr;
//	const oid default_ds_priority1_oid[] = { SNMP_OID_PTPBASE_CLOCK_DEFAULT_DS, 1, 6 };
//
//	hr = netsnmp_create_handler_registration("default_ds_priority1",
//						 handle_pdu,
//						 default_ds_priority1_oid,
//						 OID_LENGTH(default_ds_priority1_oid),
//						 HANDLER_CAN_RONLY);
//	return register_scalar(hr);
//}
//
//static int init_default_ds_priority2(void)
//{
//	netsnmp_handler_registration *hr;
//	const oid default_ds_priority2_oid[] = { SNMP_OID_PTPBASE_CLOCK_DEFAULT_DS, 1, 7 };
//
//	hr = netsnmp_create_handler_registration("default_ds_priority2",
//						 handle_pdu,
//						 default_ds_priority2_oid,
//						 OID_LENGTH(default_ds_priority2_oid),
//						 HANDLER_CAN_RONLY);
//	return register_scalar(hr);
//}
//
//static int init_port_ds_ptp_version(void)
//{
//	netsnmp_handler_registration *hr;
//	const oid port_ds_ptp_version_oid[] = { SNMP_OID_PTPBASE_CLOCK_PORT_DS, 1, 15 };
//
//	hr = netsnmp_create_handler_registration("port_ds_ptp_version",
//						 handle_pdu,
//						 port_ds_ptp_version_oid,
//						 OID_LENGTH(port_ds_ptp_version_oid),
//						 HANDLER_CAN_RONLY);
//	return register_scalar(hr);
//}

//static netsnmp_variable_list *get_current_ds(void **my_loop_context,
//					     void **my_data_context,
//					     netsnmp_variable_list
//					     *put_index_data,
//					     netsnmp_iterator_info
//					     *mydata)
//{
//	pr_err("get_current_ds");
//	if (entry_head->id != TLV_CURRENT_DATA_SET) {
//		entry_head->id = 0;
//	}
//	return get_first_data_point("GET CURRENT_DATA_SET",
//				 my_loop_context, my_data_context,
//                  		 put_index_data, mydata);
//}
//
//static netsnmp_variable_list *get_parent_ds(void **my_loop_context,
//					    void **my_data_context,
//					    netsnmp_variable_list
//					    *put_index_data,
//					    netsnmp_iterator_info
//					    *mydata)
//{
//	pr_err("get_parent_ds");
//	if (entry_head->id != TLV_PARENT_DATA_SET) {
//		entry_head->id = 0;
//	}
//	return get_first_data_point("GET PARENT_DATA_SET",
//				 my_loop_context, my_data_context,
//                  		 put_index_data, mydata);
//}
//
//static netsnmp_variable_list *get_default_ds(void **my_loop_context,
//					    void **my_data_context,
//					    netsnmp_variable_list
//					    *put_index_data,
//					    netsnmp_iterator_info
//					    *mydata)
//{
//	pr_err("get_default_ds");
//	if (entry_head->id != TLV_DEFAULT_DATA_SET) {
//		entry_head->id = 0;
//	}
//	return get_first_data_point("GET DEFAULT_DATA_SET",
//				 my_loop_context, my_data_context,
//                  		 put_index_data, mydata);
//}
//
//static netsnmp_variable_list *get_timeprop_ds(void **my_loop_context,
//					    void **my_data_context,
//					    netsnmp_variable_list
//					    *put_index_data,
//					    netsnmp_iterator_info
//					    *mydata)
//{
//	pr_err("get_timeprop_ds");
//	if (entry_head->id != TLV_TIME_PROPERTIES_DATA_SET) {
//		entry_head->id = 0;
//	}
//	return get_first_data_point("GET TIME_PROPERTIES_DATA_SET",
//				 my_loop_context, my_data_context,
//                  		 put_index_data, mydata);
//}
