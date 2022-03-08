#ifndef _INNODB_ANALYZE_H_
#define _INNODB_ANALYZE_H_

#include <stdint.h>
#include <stdbool.h>

int innodb_analyze(int argc, char **argv);

//
// system space page offset:
//   storage/innobase/include/fsp0types.h
//
#define FSP_DICT_HDR_PAGE_NO 7

//
// data dictionary header (@FSP_DICT_HDR_PAGE_NO)
//   storage/innobase/include/dict0boot.h
//
typedef struct
__attribute__((__packed__))
dd_header {
    uint64_t ddh_row_id;
    uint64_t ddh_table_id;
    uint64_t ddh_index_id;
    uint32_t ddh_max_space_id;
    uint32_t ddh_mix_id_low;
    uint32_t ddh_tables_root;
    uint32_t ddh_table_ids_root;
    uint32_t ddh_columns_root;
    uint32_t ddh_indexes_root;
    uint32_t ddh_fields_root;
} dd_header_t;

//
// page headers:
//  see storage/innobase/include/fil0types.h
//
typedef struct
__attribute__((__packed__))
fil_header {
    uint32_t fh_chksum;
    uint32_t fh_page_offset;
    uint32_t fh_page_prev;
    uint32_t fh_page_next;
    uint64_t fh_lsn;
    uint16_t fh_page_type;
    uint64_t fh_flush_lsn;
    uint32_t fh_space_id;
    uint8_t  fh_data;
} fil_header_t;

//
// page types:
//  see storage/innobase/include/fil0fil.h
//
#define FIL_PAGE_INDEX 17855
#define FIL_PAGE_RTREE 17854
#define FIL_PAGE_SDI 17853
#define FIL_PAGE_TYPE_ALLOCATED 0
#define FIL_PAGE_TYPE_UNUSED 1
#define FIL_PAGE_UNDO_LOG 2
#define FIL_PAGE_INODE 3
#define FIL_PAGE_IBUF_FREE_LIST 4
#define FIL_PAGE_IBUF_BITMAP 5
#define FIL_PAGE_TYPE_SYS 6
#define FIL_PAGE_TYPE_TRX_SYS 7
#define FIL_PAGE_TYPE_FSP_HDR 8
#define FIL_PAGE_TYPE_XDES 9
#define FIL_PAGE_TYPE_BLOB 10
#define FIL_PAGE_TYPE_ZBLOB 11
#define FIL_PAGE_TYPE_ZBLOB2 12
#define FIL_PAGE_TYPE_UNKNOWN 13
#define FIL_PAGE_COMPRESSED 14
#define FIL_PAGE_ENCRYPTED 15
#define FIL_PAGE_COMPRESSED_AND_ENCRYPTED 16
#define FIL_PAGE_ENCRYPTED_RTREE 17
#define FIL_PAGE_SDI_BLOB 18
#define FIL_PAGE_SDI_ZBLOB 19
#define FIL_PAGE_TYPE_LEGACY_DBLWR 20
#define FIL_PAGE_TYPE_RSEG_ARRAY 21
#define FIL_PAGE_TYPE_LOB_INDEX 22
#define FIL_PAGE_TYPE_LOB_DATA 23
#define FIL_PAGE_TYPE_LOB_FIRST 24
#define FIL_PAGE_TYPE_ZLOB_FIRST 25
#define FIL_PAGE_TYPE_ZLOB_DATA 26
#define FIL_PAGE_TYPE_ZLOB_INDEX 27
#define FIL_PAGE_TYPE_ZLOB_FRAG 28
#define FIL_PAGE_TYPE_ZLOB_FRAG_ENTRY 29

//
// FSP_HDR pages:
//  see storage/innobase/include/fsp0fsp.h
//
typedef struct
__attribute__((__packed__))
fsp_header {
    uint32_t fsp_space_id;
    uint32_t fsp_not_used;
    uint32_t fsp_size;
    uint32_t fsp_free_limit;
    uint32_t fsp_space_flags;
    // ...
} fsp_header_t;

//
// FIL_PAGE_INDEX pages:
//  see storage/innobase/include/page0types.h
//

#define IPH_FORMAT_BIT 0x8000
#define IPH_N_HEAP_RECS_BITS (IPH_FORMAT_BIT - 1)

typedef struct
__attribute__((__packed__))
index_page_header {
    uint16_t iph_n_dir_slots;
    uint16_t iph_heap_top;
    uint16_t iph_n_heap_recs_and_format;
    uint16_t iph_free_records_list;
    uint16_t iph_garbage_space;
    uint16_t iph_last_insert;
    uint16_t iph_direction;
    uint16_t iph_n_direction;
    uint16_t iph_n_records;
    uint64_t iph_max_txn_id;
    uint16_t iph_level;
    uint64_t iph_index_id;
} index_page_header_t;

#define IR_INFO_FLAGS_MASK 0xF
#define IR_RECORDS_OWNED_SHIFT 4
#define IR_ORDER_MASK 0x1FFF
#define IR_RECORD_TYPE_SHIFT 13

typedef struct
__attribute__((__packed__))
system_record {
    uint8_t sr_info_flags_and_records_owned;
    uint16_t sr_order_and_record_type;
    uint16_t sr_next_record_offset;
    uint64_t sr_name_string;
} system_record_t;

typedef struct
__attribute__((__packed__))
system_records {
    system_record_t srs_infimum;
    system_record_t srs_supremum;
} system_records_t;

#define UR_NONVAR_SIZE 5

typedef struct
__attribute__((__packed__))
user_record {
    // uint8_t ur_variable_field_lengths[]
    // uint8_t ur_nullable_field_bitmap[]
    uint8_t ur_info_flags_and_records_owned;
    uint16_t ur_order_and_record_type;
    int16_t ur_next_record_offset;
    uint8_t ur_data;
} user_record_t;


//
// system table/index (ie. tables/indexes that live in mysql.ibd) definitions:
//

// from storage/innobase/include/data0type.h:
typedef enum {
    COLUMN_TYPE_MISSING,
    COLUMN_TYPE_VARCHAR,
    COLUMN_TYPE_CHAR,
    COLUMN_TYPE_FIXBINARY,
    COLUMN_TYPE_BINARY,
    COLUMN_TYPE_BLOB,
    COLUMN_TYPE_INT,
    COLUMN_TYPE_SYSCHILD,
    COLUMN_TYPE_SYS,
    COLUMN_TYPE_FLOAT,
    COLUMN_TYPE_DOUBLE,
    COLUMN_TYPE_DECIMAL,
    COLUMN_TYPE_VARMYSQL,
    COLUMN_TYPE_MYSQL,
    COLUMN_TYPE_GEOMETRY,
    COLUMN_TYPE_POINT,
    COLUMN_TYPE_VAR_POINT,
    COLUMN_TYPE_NONE
} column_type_t;

// from dtype_get_fixed_size_low in storage/innobase/include/data0type.ic:
static inline bool cl_is_variable_length(column_type_t ct) {
    switch (ct) {
        case COLUMN_TYPE_VARCHAR:
        case COLUMN_TYPE_BINARY:
        case COLUMN_TYPE_DECIMAL:
        case COLUMN_TYPE_VARMYSQL:
        case COLUMN_TYPE_VAR_POINT:
        case COLUMN_TYPE_GEOMETRY:
        case COLUMN_TYPE_BLOB:
            return true;
        default:
            return false;
    }
}

typedef struct column {
    const char *cl_name;
    column_type_t cl_type;
    uint16_t cl_length;
    bool cl_nullable;
} column_t;

typedef enum {
    INDEX_DEF_TYPE_COLUMNS
} index_def_type_t;

typedef struct index_def {
    column_t *id_columns;
    uint8_t id_ncolumns;
    uint8_t id_nnullable;
} index_def_t;

//
// from sql/dd/impl/tables/columns.cc and from printfs in
// row_sel_store_mysql_rec and row_sel_store_mysql_field_func
//

#define ST_COLUMNS_NCOLUMNS 34
#define ST_COLUMNS_NNULLABLE 22

#endif /* _INNODB_ANALYZE_H_ */
