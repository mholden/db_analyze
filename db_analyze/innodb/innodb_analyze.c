#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <endian.h>

#define ntohll be64toh

#include "innodb_analyze.h"

static int file;
static void *buf;

static uint64_t bmwidth=128;
static uint64_t bufsz=16384;

//
// system table/index (ie. tables/indexes that live in mysql.ibd) definitions:
//

//
// from sql/dd/impl/tables/columns.cc and from printfs in
// row_sel_store_mysql_rec and row_sel_store_mysql_field_func
//

column_t st_columns_columns[] = {
    { .cl_name = "id", .cl_type = COLUMN_TYPE_INT, .cl_length = 8, .cl_nullable = false},
    { .cl_name = "DB_TRX_ID", .cl_type = COLUMN_TYPE_NONE, .cl_length = 6, .cl_nullable = false},
    { .cl_name = "DB_ROLL_PTR", .cl_type = COLUMN_TYPE_NONE, .cl_length = 7, .cl_nullable = false},
    { .cl_name = "table_id", .cl_type = COLUMN_TYPE_INT, .cl_length = 8, .cl_nullable = false},
    { .cl_name = "name", .cl_type = COLUMN_TYPE_VARMYSQL, .cl_length = 0, .cl_nullable = false},
    { .cl_name = "ordinal_position", .cl_type = COLUMN_TYPE_INT, .cl_length = 4, .cl_nullable = false},
    { .cl_name = "type", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = false},
    { .cl_name = "is_nullable", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = false},
    { .cl_name = "is_zerofill", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = true},
    { .cl_name = "is_unsigned", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = true},
    { .cl_name = "char_length", .cl_type = COLUMN_TYPE_INT, .cl_length = 4, .cl_nullable = true},
    { .cl_name = "numeric_precision", .cl_type = COLUMN_TYPE_INT, .cl_length = 4, .cl_nullable = true},
    { .cl_name = "numeric_scale", .cl_type = COLUMN_TYPE_INT, .cl_length = 4, .cl_nullable = true},
    { .cl_name = "datatime_precision", .cl_type = COLUMN_TYPE_INT, .cl_length = 4, .cl_nullable = true},
    { .cl_name = "collation_id", .cl_type = COLUMN_TYPE_INT, .cl_length = 8, .cl_nullable = true},
    { .cl_name = "has_no_default", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = true},
    { .cl_name = "default_value", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "default_value_utf8", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "default_option", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "update_option", .cl_type = COLUMN_TYPE_INT, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "is_auto_increment", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = true},
    { .cl_name = "is_virtual", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = true},
    { .cl_name = "generation_expression", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "generation_expression_utf8", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "comment", .cl_type = COLUMN_TYPE_VARMYSQL, .cl_length = 0, .cl_nullable = false},
    { .cl_name = "hidden", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = false},
    { .cl_name = "options", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "se_private_data", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "column_key", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = false},
    { .cl_name = "column_type_utf8", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = false},
    { .cl_name = "srs_id", .cl_type = COLUMN_TYPE_INT, .cl_length = 4, .cl_nullable = true},
    { .cl_name = "is_explicit_collation", .cl_type = COLUMN_TYPE_INT, .cl_length = 1, .cl_nullable = true},
    { .cl_name = "engine_attribute", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true},
    { .cl_name = "secondary_engine_attribute", .cl_type = COLUMN_TYPE_BLOB, .cl_length = 0, .cl_nullable = true}
};

index_def_t std_columns = {
    .id_columns = st_columns_columns,
    .id_ncolumns = ST_COLUMNS_NCOLUMNS,
    .id_nnullable = ST_COLUMNS_NNULLABLE
};

index_def_t *system_index_defs[] = {
    &std_columns,
    NULL
};

// called directly from cmd_table
static void analyze_fsp_header(int argc, char **argv);
static void analyze_pages(int argc, char **argv);
static void analyze_index(int argc, char **argv);
static void set_bmwidth(int argc, char **argv);
static void set_bufsz(int argc, char **argv);
//static void analyze_bm(int argc, char **argv);
/*static void analyze_journal(int argc, char **argv);
static void analyze_allocation_file(int argc, char **argv);
static void analyze_catalog_file(int argc, char **argv);
static void analyze_attributes_file(int argc, char **argv);
static void analyze_extents_file(int argc, char **argv);
static void analyze_catalog_records(int argc, char **argv);
static void analyze_attributes_records(int argc, char **argv);
static void analyze_xattrs(int argc, char **argv);
static void analyze_btree_node(int argc, char **argv);
static void scan_btree(int argc, char **argv);
static void analyze_disk(int argc, char **argv);*/


// not called directly from cmd_table
/*static void _analyze_inode(struct ext2_inode_large *ino, uint32_t bsize);
static void _analyze_header_records(void *btn, HFSPlusForkData fd, uint32_t blksz, bool map);
static void _analyze_index_records(void *btn);
static void _analyze_leaf_records(void *btn);
static void _analyze_file_record(HFSPlusCatalogKey *key, HFSPlusCatalogFile *record);
static void _analyze_folder_record(HFSPlusCatalogKey *key, HFSPlusCatalogFolder *record);
static void _analyze_thread_record(HFSPlusCatalogKey *key, HFSPlusCatalogThread *record);
static void _analyze_xattr(HFSPlusAttrKey *key, HFSPlusAttrRecord *record);
static void _analyze_fork_data(HFSPlusForkData *fd);
static void _analyze_inline_attr_record(HFSPlusAttrData *data);
static void _analyze_attr_fork_record(HFSPlusAttrForkData *forkdata);
static void _analyze_bm(uint64_t addr, uint32_t blksz, int32_t bit);
static void __analyze_bm(uint8_t *bm, uint32_t bmsz, int32_t bit);
static void _analyze_btree_node(void *btn, uint32_t blksz);
static void _analyze_btree_file(HFSPlusForkData fd, uint32_t blksz, bool map, bool dump);*/

struct command {
	const char *name;
	void (*func)(int, char **);
};

static struct command cmd_table[] = {
	{"fsp", analyze_fsp_header},
    {"pages", analyze_pages},
    {"index", analyze_index},
	{"bmwidth", set_bmwidth},
	{"bufsz", set_bufsz},
	{NULL, NULL}
};

#define DOUBLE_QUOTE '"'
#define SINGLE_QUOTE '\''
#define BACK_SLASH   '\\'

static char **
build_argv(char *str, int *argc)
{
	int table_size = 16, _argc;
	char **argv;
	
	if (argc == NULL)
		argc = &_argc;
	
	*argc = 0;
	argv = (char **)calloc(table_size, sizeof(char *));
	
	if (argv == NULL)
		return NULL;
	
	while (*str) {
		// skip intervening white space //
		while (*str != '\0' && (*str == ' ' || *str == '\t' || *str == '\n'))
			str++;
		
		if (*str == '\0')
			break;
		
		if (*str == DOUBLE_QUOTE) {
			argv[*argc] = ++str;
			while (*str && *str != DOUBLE_QUOTE) {
				if (*str == BACK_SLASH)
					memmove(str, str + 1, strlen(str+1)+1); // copy everything down //
				str++;
			}
		} else if (*str == SINGLE_QUOTE) {
			argv[*argc] = ++str;
			while (*str && *str != SINGLE_QUOTE) {
				if (*str == BACK_SLASH)
					memmove(str, str + 1, strlen(str+1)+1); // copy everything down //
				str++;
			}
		} else {
			argv[*argc] = str;
			while (*str && *str != ' ' && *str != '\t' && *str != '\n') {
				if (*str == BACK_SLASH)
					memmove(str, str + 1, strlen(str+1)+1); // copy everything down //
				str++;
			}
		}
		
		if (*str != '\0')
			*str++ = '\0'; // chop the string //
		
		*argc = *argc + 1;
		if (*argc >= table_size - 1) {
			char **nargv;
			
			table_size = table_size * 2;
			nargv = (char **)calloc(table_size, sizeof(char *));
			
			if (nargv == NULL) { // drats! failure. //
				free(argv);
				return NULL;
			}
			
			memcpy(nargv, argv, (*argc) * sizeof(char *));
			free(argv);
			argv = nargv;
		}
	}
	
	return argv;
}

static void set_bmwidth(int argc, char **argv) {
	bmwidth = strtoull(argv[1], NULL, 0);
	return;
}

static void set_bufsz(int argc, char **argv) {
	bufsz = strtoull(argv[1], NULL, 0);
	buf = realloc(buf, bufsz);
	if (buf == NULL)
		printf("error realloc'ing buf to bufsz %" PRIu64 "\n", bufsz);
	else
		printf("realloc'd buf to bufsz %" PRIu64 "\n", bufsz);
	return;
}

// other utility functions

static const char * _page_type_to_string(uint16_t page_type) {
    switch (page_type) {
        case FIL_PAGE_INDEX:
            return "FIL_PAGE_INDEX";
        case FIL_PAGE_RTREE:
            return "FIL_PAGE_RTREE";
        case FIL_PAGE_SDI:
            return "FIL_PAGE_SDI";
        case FIL_PAGE_TYPE_ALLOCATED:
            return "FIL_PAGE_TYPE_ALLOCATED";
        case FIL_PAGE_TYPE_UNUSED:
            return "FIL_PAGE_TYPE_UNUSED";
        case FIL_PAGE_UNDO_LOG:
            return "FIL_PAGE_UNDO_LOG";
        case FIL_PAGE_INODE:
            return "FIL_PAGE_INODE";
        case FIL_PAGE_IBUF_FREE_LIST:
            return "FIL_PAGE_IBUF_FREE_LIST";
        case FIL_PAGE_IBUF_BITMAP:
            return "FIL_PAGE_IBUF_BITMAP";
        case FIL_PAGE_TYPE_SYS:
            return "FIL_PAGE_TYPE_SYS";
        case FIL_PAGE_TYPE_TRX_SYS:
            return "FIL_PAGE_TYPE_TRX_SYS";
        case FIL_PAGE_TYPE_FSP_HDR:
            return "FIL_PAGE_TYPE_FSP_HDR";
        case FIL_PAGE_TYPE_XDES:
            return "FIL_PAGE_TYPE_XDES";
        case FIL_PAGE_TYPE_BLOB:
            return "FIL_PAGE_TYPE_BLOB";
        case FIL_PAGE_TYPE_ZBLOB:
            return "FIL_PAGE_TYPE_ZBLOB";
        case FIL_PAGE_TYPE_ZBLOB2:
            return "FIL_PAGE_TYPE_ZBLOB2";
        case FIL_PAGE_TYPE_UNKNOWN:
            return "FIL_PAGE_TYPE_UNKNOWN";
        case FIL_PAGE_COMPRESSED:
            return "FIL_PAGE_COMPRESSED";
        case FIL_PAGE_ENCRYPTED:
            return "FIL_PAGE_ENCRYPTED";
        case FIL_PAGE_COMPRESSED_AND_ENCRYPTED:
            return "FIL_PAGE_COMPRESSED_AND_ENCRYPTED";
        case FIL_PAGE_ENCRYPTED_RTREE:
            return "FIL_PAGE_ENCRYPTED_RTREE";
        case FIL_PAGE_SDI_BLOB:
            return "FIL_PAGE_SDI_BLOB";
        case FIL_PAGE_SDI_ZBLOB:
            return "FIL_PAGE_SDI_ZBLOB";
        case FIL_PAGE_TYPE_LEGACY_DBLWR:
            return "FIL_PAGE_TYPE_LEGACY_DBLWR";
        case FIL_PAGE_TYPE_RSEG_ARRAY:
            return "FIL_PAGE_TYPE_RSEG_ARRAY";
        case FIL_PAGE_TYPE_LOB_INDEX:
            return "FIL_PAGE_TYPE_LOB_INDEX";
        case FIL_PAGE_TYPE_LOB_DATA:
            return "FIL_PAGE_TYPE_LOB_DATA";
        case FIL_PAGE_TYPE_LOB_FIRST:
            return "FIL_PAGE_TYPE_LOB_FIRST";
        case FIL_PAGE_TYPE_ZLOB_FIRST:
            return "FIL_PAGE_TYPE_ZLOB_FIRST";
        case FIL_PAGE_TYPE_ZLOB_DATA:
            return "FIL_PAGE_TYPE_ZLOB_DATA";
        case FIL_PAGE_TYPE_ZLOB_INDEX:
            return "FIL_PAGE_TYPE_ZLOB_INDEX";
        case FIL_PAGE_TYPE_ZLOB_FRAG:
            return "FIL_PAGE_TYPE_ZLOB_FRAG";
        case FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY:
            return "FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY";
        default:
            //printf("page type %" PRIu16 "\n", page_type);
            return "UNKNOWN";
    }
}

static int32_t _page_string_to_type(char *page_type) {
    if (!strcmp(page_type, "index"))
        return FIL_PAGE_INDEX;
    else if (!strcmp(page_type, "rtree"))
        return FIL_PAGE_RTREE;
    else if (!strcmp(page_type, "sdi"))
        return FIL_PAGE_SDI;
    else if (!strcmp(page_type, "allocated"))
        return FIL_PAGE_TYPE_ALLOCATED;
    else if (!strcmp(page_type, "unused"))
        return FIL_PAGE_TYPE_UNUSED;
    else if (!strcmp(page_type, "log"))
        return FIL_PAGE_UNDO_LOG;
    else if (!strcmp(page_type, "inode"))
        return FIL_PAGE_INODE;
    else if (!strcmp(page_type, "ibuf_free_list"))
        return FIL_PAGE_IBUF_FREE_LIST;
    else if (!strcmp(page_type, "ibuf_bitmap"))
        return FIL_PAGE_IBUF_BITMAP;
    else if (!strcmp(page_type, "sys"))
        return FIL_PAGE_TYPE_SYS;
    else if (!strcmp(page_type, "trx_sys"))
        return FIL_PAGE_TYPE_TRX_SYS;
    else if (!strcmp(page_type, "fsp_hdr"))
        return FIL_PAGE_TYPE_FSP_HDR;
    else if (!strcmp(page_type, "xdes"))
        return FIL_PAGE_TYPE_XDES;
    else if (!strcmp(page_type, "blob"))
        return FIL_PAGE_TYPE_BLOB;
    else if (!strcmp(page_type, "zblob"))
        return FIL_PAGE_TYPE_ZBLOB;
    else if (!strcmp(page_type, "zblob2"))
        return FIL_PAGE_TYPE_ZBLOB2;
    else if (!strcmp(page_type, "unknown"))
        return FIL_PAGE_TYPE_UNKNOWN;
    else if (!strcmp(page_type, "compressed"))
        return FIL_PAGE_COMPRESSED;
    else if (!strcmp(page_type, "encrypted"))
        return FIL_PAGE_ENCRYPTED;
    else if (!strcmp(page_type, "compressed_and_encrypted"))
        return FIL_PAGE_COMPRESSED_AND_ENCRYPTED;
    else if (!strcmp(page_type, "encrypted_rtree"))
        return FIL_PAGE_ENCRYPTED_RTREE;
    else if (!strcmp(page_type, "sdi_blob"))
        return FIL_PAGE_SDI_BLOB;
    else if (!strcmp(page_type, "sdi_zblob"))
        return FIL_PAGE_SDI_ZBLOB;
    else if (!strcmp(page_type, "legacy_dblwr"))
        return FIL_PAGE_TYPE_LEGACY_DBLWR;
    else if (!strcmp(page_type, "rseg_array"))
        return FIL_PAGE_TYPE_RSEG_ARRAY;
    else if (!strcmp(page_type, "lob_index"))
        return FIL_PAGE_TYPE_LOB_INDEX;
    else if (!strcmp(page_type, "lob_data"))
        return FIL_PAGE_TYPE_LOB_DATA;
    else if (!strcmp(page_type, "lob_first"))
        return FIL_PAGE_TYPE_LOB_FIRST;
    else if (!strcmp(page_type, "zlob_first"))
        return FIL_PAGE_TYPE_ZLOB_FIRST;
    else if (!strcmp(page_type, "zlob_data"))
        return FIL_PAGE_TYPE_ZLOB_DATA;
    else if (!strcmp(page_type, "zlob_index"))
        return FIL_PAGE_TYPE_ZLOB_INDEX;
    else if (!strcmp(page_type, "zlob_frag"))
        return FIL_PAGE_TYPE_ZLOB_FRAG;
    else if (!strcmp(page_type, "zlob_frag_entry"))
        return FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY;
    else
        return -1;
}

static index_def_type_t _index_def_string_to_type(char *idef) {
    if (!strcmp(idef, "columns"))
        return INDEX_DEF_TYPE_COLUMNS;
    else
        return -1;
}

static bool cl_is_null(uint8_t *nulls, uint64_t mask) {
    uint8_t bit = 0, byte = 0;
    uint64_t _mask = mask;
    while (_mask) {
        bit++;
        _mask >>= 1;
    }
    bit--;
    byte = bit / 8;
    nulls -= byte;
    mask >>= byte * 8;
    return (*nulls & mask);
}

// _<structure>_to_buf functions

static int _fsp_header_to_buf(void) {
    //fil_header_t fh;
    //fsp_header_t *fsp;
    //uint32_t blksz;
	
	if (pread(file, buf, bufsz, 0) != bufsz) {
		printf("pread() error.\n");
		return -1;
	}
    
    /*fh = (fil_header_t *)buf;
    if (blksz > bufsz)
        printf("WARNING: fs block size (%" PRIu32 ") > bufsz (%" PRIu64 ")\n", blksz, bufsz);
    if (!(sb->s_feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT)) {
        // we assume the high bits on 64-bit fields are used throughout this program
        printf("WARNING: !EXT4_FEATURE_INCOMPAT_64BIT currently unsupported\n");
    }*/
    
	return 0;
}

// analyze_<structure> functions

static void __analyze_bm(uint8_t *bm, uint32_t bmsz) {
    int ind=0, ones=0;
    
    while (ind < bmsz) {
        for (int i=0; i < (bmwidth / 2); i++) {
            for (int j=0; j < 8; j++) {
                if (bm[ind] & (1<<j))
                    ones++;
            }
            printf("%02x", bm[ind]);
            if (++ind >= bmsz)
                break;
        }
        printf("\n");
    }
    printf("bits set: %d\n", ones);
    
    printf("\n");
}

static void _analyze_bm(uint64_t addr, uint32_t bmsize, uint32_t blksz) {
    uint8_t *bm=NULL;
    
    bm = malloc(blksz);
    if (bm == NULL) {
        printf("malloc error\n");
        goto out;
    }
    
    if (pread(file, bm, blksz, addr * blksz) != blksz) {
        printf("pread() error.\n");
        goto out;
    }
    
    __analyze_bm(bm, bmsize);
    
out:
    if (bm)
        free(bm);
    
    return;
}

static void _analyze_fil_header(fil_header_t *fh) {
    printf("fil_header:\n");
    printf("  fh_page_type: %" PRIu16 " (%s)\n", ntohs(fh->fh_page_type), _page_type_to_string(ntohs(fh->fh_page_type)));
    printf("  fh_page_offset: %" PRIu32 "\n", ntohl(fh->fh_page_offset));
    printf("\n");
}

static void _analyze_index_page_header(index_page_header_t *iph) {
    printf("index_page_header:\n");
    printf("  iph_n_dir_slots: %" PRIu16 "\n", ntohs(iph->iph_n_dir_slots));
    printf("  iph_heap_top: %" PRIu16 "\n", ntohs(iph->iph_heap_top));
    printf("  iph_n_heap_recs: %" PRIu16 "\n", (uint16_t)(ntohs(iph->iph_n_heap_recs_and_format) & IPH_N_HEAP_RECS_BITS));
    printf("  iph_format: %s\n", (ntohs(iph->iph_n_heap_recs_and_format) & IPH_FORMAT_BIT) ? "COMPACT" : "REDUNDANT");
    printf("  iph_free_records_list: %" PRIu16 "\n", ntohs(iph->iph_free_records_list));
    printf("  iph_n_records: %" PRIu16 "\n", ntohs(iph->iph_n_records));
    printf("  iph_level: %" PRIu16 "\n", ntohs(iph->iph_level));
    printf("  iph_index_id: %" PRIu64 "\n", ntohll(iph->iph_index_id));
    printf("\n");
}

static void _analyze_index_page_system_record(system_record_t *sr) {
    printf("%s system record:\n", (char *)&sr->sr_name_string);
    printf("  sr_info_flags: 0x%" PRIX8 "\n", (uint8_t)(sr->sr_info_flags_and_records_owned & IR_INFO_FLAGS_MASK));
    printf("  sr_records_owned: %" PRIu8 "\n", (uint8_t)(sr->sr_info_flags_and_records_owned >> IR_RECORDS_OWNED_SHIFT));
    printf("  sr_record_type: %" PRIu16 "\n", (uint16_t)(ntohs(sr->sr_order_and_record_type) >> IR_RECORD_TYPE_SHIFT));
    printf("  sr_next_record_offset: %" PRIu16 "\n", ntohs(sr->sr_next_record_offset));
    printf("\n");
}

static void _analyze_index_page_user_record(user_record_t *ur, int n, index_def_type_t idef) {
    index_def_t *id;
    uint8_t *nulls, *lens, len;
    uint32_t curr_off, off;
    uint64_t null_mask = 1;
    
    printf("user record %d (page offset %lu):\n", n, ((char *)&ur->ur_data - (char *)buf));
    printf("  ur_info_flags: 0x%" PRIX8 "\n", (uint8_t)(ur->ur_info_flags_and_records_owned & IR_INFO_FLAGS_MASK));
    printf("  ur_records_owned: %" PRIu8 "\n", (uint8_t)(ur->ur_info_flags_and_records_owned >> IR_RECORDS_OWNED_SHIFT));
    printf("  ur_record_type: %" PRIu16 "\n", (uint16_t)(ntohs(ur->ur_order_and_record_type) >> IR_RECORD_TYPE_SHIFT));
    printf("  ur_next_record_offset: %" PRId16 "\n", (int16_t)ntohs(ur->ur_next_record_offset));
    if (idef >= 0) {
        printf("  ur_columns:");
        id = system_index_defs[idef];
        if (id->id_nnullable > 64) {
            printf("id_nnullable > 64 not yet supported\n");
            goto out;
        }
        nulls = (uint8_t *)ur - 1;
        lens = nulls - ((id->id_nnullable + 7) / 8);
        curr_off = 0;
        for (int i = 0; i < id->id_ncolumns; i++) {
            printf("\n    %s: ", id->id_columns[i].cl_name);
            if (id->id_columns[i].cl_nullable) {
                if (cl_is_null(nulls, null_mask)) {
                    printf("null");
                    null_mask <<= 1;
                    continue;
                }
                null_mask <<= 1;
            }
            // not null
            if (id->id_columns[i].cl_type == COLUMN_TYPE_VARMYSQL) {
                off = curr_off;
                len = *lens;
                while (len--)
                    printf("%c", *((char *)&ur->ur_data + off++));
            }
            if (id->id_columns[i].cl_length)
                curr_off += id->id_columns[i].cl_length;
            else
                curr_off += *lens--;
        }
        printf("\n");
    }
    
out:
    printf("\n");
    return;
}

static void _analyze_index_page(fil_header_t *fh, index_def_type_t idef) {
    index_page_header_t *iph;
    system_records_t *srs;
    user_record_t *ur;
    
    iph = (index_page_header_t *)&fh->fh_data;
    _analyze_index_page_header(iph);
    
#define FSEG_HEADER_SIZE 20 // TODO: print this too
    
    srs = (system_records_t *)((uint8_t *)iph + sizeof(index_page_header_t) + FSEG_HEADER_SIZE);
    _analyze_index_page_system_record(&srs->srs_infimum);
    _analyze_index_page_system_record(&srs->srs_supremum);
    
    ur = (user_record_t *)((char *)&srs->srs_infimum.sr_name_string + ntohs(srs->srs_infimum.sr_next_record_offset) - UR_NONVAR_SIZE);
    for (int i = 0; i < ntohs(iph->iph_n_records); i++) {
        _analyze_index_page_user_record(ur, i, idef);
        ur = (user_record_t *)((char *)&ur->ur_data + ntohs(ur->ur_next_record_offset) - UR_NONVAR_SIZE);
    }
    
    return;
}

// dump a bitmap block given any paddr
/*static void analyze_bm(int argc, char **argv) {
    struct ext2_super_block *sb;
    int64_t addr = -1;
    int32_t bmsize = -1, blksize = -1;
    int ch;
    
    struct option longopts[] = {
        { "addr",      required_argument,    NULL,    'a' },
        { "bmsize",    required_argument,    NULL,    's' },
        { NULL,            0,                NULL,    0 }
    };
    
    optind = 1;
    while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (ch) {
            case 'a':
                addr = strtoull(optarg, NULL, 0);
                break;
            case 's':
                bmsize = atoi(optarg);
                break;
            default:
                printf("usage: %s --addr <bm addr> [--bmsize <bmsize (in bits)>]\n", argv[0]);
                return;
        }
    }
    
    if (addr < 0) {
        printf("usage: %s --addr <bm addr> [--bmsize <bmsize (in bits)>]\n", argv[0]);
        return;
    }
    
    if (_superblock_to_buf())
        goto out;
    
    sb = (struct ext2_super_block *)buf;
    blksize = (int32_t)(1 << (10 + sb->s_log_block_size));
    
    if (bmsize < 0) {
        bmsize = blksize;
        printf("using volume blocksize (%d) as bmsize\n", blksize);
    } else {
        if (bmsize % 8)
            printf("WARNING: bmsize not divisible by 8, truncating now\n");
        bmsize /= 8;
    }
    _analyze_bm(addr, bmsize, blksize);
    
out:
    return;
}*/

static void analyze_index(int argc, char **argv) {
    fil_header_t *fh;
    index_page_header_t *iph;
    int64_t id = -1;
    int32_t pno;
    int ch;
    
    struct option longopts[] = {
        { "id",       required_argument,    NULL,    'i' },
        { NULL,            0,               NULL,     0 }
    };
    
    optind = 1;
    while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (ch) {
            case 'i':
                id = (int32_t)strtoll(optarg, NULL, 0);
                break;
            default:
                printf("usage: %s --id <index id>\n", argv[0]);
                return;
        }
    }
    
    if (id < 0) {
        printf("usage: %s --id <index id>\n", argv[0]);
        goto out;
    }
    
    // just dump all pages of the index
    pno = 0; // TODO: get page size first and then do pno * pagesz
    while (pread(file, buf, bufsz, pno * bufsz) == bufsz) {
        fh = (fil_header_t *)buf;
        if (ntohs(fh->fh_page_type) == FIL_PAGE_INDEX) {
            iph = (index_page_header_t *)&fh->fh_data;
            if (ntohll(iph->iph_index_id) == id) {
                printf("page %d: %s id %" PRIu64 " level %" PRIu16 "\n", pno, _page_type_to_string(ntohs(fh->fh_page_type)), ntohll(iph->iph_index_id), ntohs(iph->iph_level));
            }
        }
        pno++;
    }
    printf("\n");
    
out:
    return;
}

static void analyze_pages(int argc, char **argv) {
    fil_header_t *fh;
    int32_t pno = -1, _pno, ptype = -1, idef = -1;
    bool all = false;
    int ch;
    
    struct option longopts[] = {
        { "pno",      required_argument,    NULL,    'p' },
        { "all",      no_argument,          NULL,    'a' },
        { "type",     required_argument,    NULL,    't' },
        { "idef",     required_argument,    NULL,    'i' },
        { NULL,            0,               NULL,     0 }
    };
    
    optind = 1;
    while ((ch = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (ch) {
            case 'p':
                pno = (int32_t)strtol(optarg, NULL, 0);
                break;
            case 'a':
                all = true;
                break;
            case 't':
                ptype = _page_string_to_type(optarg);
                break;
            case 'i':
                idef = _index_def_string_to_type(optarg);
                break;
            default:
                printf("usage: %s --pno <page number> [--idef <index definition>] | --all | --type <page type>\n", argv[0]);
                return;
        }
    }
    
    if (all) { // dump all pages
        printf("\n***** all pages *****\n");
        
        _pno = 0; // TODO: get page size first and then do pno * pagesz
        while (pread(file, buf, bufsz, _pno * bufsz) == bufsz) {
            fh = (fil_header_t *)buf;
            printf("page %d: %s\n", _pno, _page_type_to_string(ntohs(fh->fh_page_type)));
            _pno++;
        }
        
        printf("\n");
    } else if (pno >= 0) { // dump single page
        // TODO: get page size first and then do pno * pagesz
        if (pread(file, buf, bufsz, pno * bufsz) != bufsz) {
            printf("pread() error.\n");
            goto out;
        }
        
        fh = (fil_header_t *)buf;
        
        printf("\n***** page %" PRId32 " *****\n", pno);
        _analyze_fil_header(fh);
        
        switch (ntohs(fh->fh_page_type)) {
            case FIL_PAGE_INDEX:
                _analyze_index_page(fh, idef);
                return;
            case FIL_PAGE_RTREE:
            case FIL_PAGE_SDI:
            case FIL_PAGE_TYPE_ALLOCATED:
            case FIL_PAGE_TYPE_UNUSED:
            case FIL_PAGE_UNDO_LOG:
            case FIL_PAGE_INODE:
            case FIL_PAGE_IBUF_FREE_LIST:
            case FIL_PAGE_IBUF_BITMAP:
            case FIL_PAGE_TYPE_SYS:
            case FIL_PAGE_TYPE_TRX_SYS:
            case FIL_PAGE_TYPE_FSP_HDR:
            case FIL_PAGE_TYPE_XDES:
            case FIL_PAGE_TYPE_BLOB:
            case FIL_PAGE_TYPE_ZBLOB:
            case FIL_PAGE_TYPE_ZBLOB2:
            case FIL_PAGE_TYPE_UNKNOWN:
            default:
                return;
        }
        
    } else if (ptype >= 0) {
        _pno = 0; // TODO: get page size first and then do pno * pagesz
        while (pread(file, buf, bufsz, _pno * bufsz) == bufsz) {
            fh = (fil_header_t *)buf;
            if (ntohs(fh->fh_page_type) == ptype) {
                printf("page %d: %s\n", _pno, _page_type_to_string(ntohs(fh->fh_page_type)));
            }
            _pno++;
        }
    } else {
        printf("usage: %s --pno <page number> [--idef <index definition>] | --all | --type <page type>\n", argv[0]);
    }

out:
    return;
}

static void analyze_fsp_header(int argc, char **argv) {
    fil_header_t *fh;
	fsp_header_t *fsp;
	
	if (_fsp_header_to_buf())
		goto out;
	
	fh = (fil_header_t *)buf;
    
	printf("\n***** fsp header *****\n");
    _analyze_fil_header(fh);
    
    fsp = (fsp_header_t *)&fh->fh_data;
    printf("fsp_space_id: %" PRIu32 " (0x%" PRIX32 ")\n", ntohl(fsp->fsp_space_id), ntohl(fsp->fsp_space_id));
    printf("fsp_size: %" PRIu32 "\n", ntohl(fsp->fsp_size));
    printf("fsp_space_flags: 0x%" PRIX32 "\n", ntohl(fsp->fsp_space_flags));
    printf("\n");
    
out:
	return;
}

int innodb_analyze(int argc, char **argv) {
	char *database_path;
	char in[256] = { 0 };
	int _argc;
	char **_argv = NULL;
	struct command *cmd;
	int error=0;
	
    if (argv[1]) {
        database_path = argv[1];
    } else {
        printf("usage: %s <database-file> <system-space-file>\n", argv[0]);
        return -1;
    }
        
	file = open(database_path, O_RDONLY);
	if (file == -1) {
		printf("open error: %s\n", strerror(errno));
		return -1;
	}
	
	buf = malloc(bufsz);
	if (buf == NULL) {
		printf("malloc error\n");
		goto out;
	}
	
	while (1) {
		printf(">>");
		gets(in);
		
		if (strcmp(in, "quit") == 0)
			break;
		
		_argv = build_argv(in, &_argc);
		if (_argv == NULL || _argc == 0)
			continue;
		
		int i;
		for (i=0, cmd = &cmd_table[i]; cmd->name; cmd = &cmd_table[++i]) {
			if (strcmp(cmd->name, _argv[0]) == 0) {
				cmd->func(_argc, _argv);
				break;
			}
		}
		
		if (!cmd->name) {
			printf("unknown command: %s\n", _argv[0]);
			printf("known commands:\n");
			for (i=0, cmd = &cmd_table[i]; cmd->name; cmd = &cmd_table[++i]) {
				printf("%s\n", cmd->name);
			}
		}
		
		
		free(_argv);
	}
	
out:
	if (buf)
		free(buf);
	close(file);
	
	return error;
}
