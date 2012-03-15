/* Copyright (C) 2011 Nokia Corporation.
 * Author: Lauri T. Aarnio
*/

/* A Rule Tree is a shared-memory repository of mapping-
 * and other kind of rules for SB2.
 *
 * The rule tree is created when a session is created,
 * by writing entries to a file. Once created, it isn't
 * modified anymore - the client processes will mmap() it
 * to memory, and use it (R/O) without locking anything.
 *
 * Since the file may be mapped to any address, pointers
 * can not be used in the rule tree. Instead, offsets 
 * (relative to the beginning of the file) are used.
*/

#ifndef SB2_RULETREE_H__
#define SB2_RULETREE_H__

/* object offset must be an unsigned type: */
typedef uint32_t ruletree_object_offset_t;

/* Every object in the rule tree begins with an object header:
 * (all structures contain this as the first member, etc) */
typedef struct ruletree_object_hdr_s {
	uint32_t	rtree_obj_magic;
	uint32_t	rtree_obj_type;
} ruletree_object_hdr_t;

#define SB2_RULETREE_MAGIC	0xE7A801FF

/* there are four different types of objects: */
#define SB2_RULETREE_OBJECT_TYPE_FILEHDR	1	/* the very first thing */
#define SB2_RULETREE_OBJECT_TYPE_CATALOG	2	/* ruletree_catalog_entry_t */
#define SB2_RULETREE_OBJECT_TYPE_FSRULE		3	/* ruletree_fsrule_t */
#define SB2_RULETREE_OBJECT_TYPE_STRING		4	/* ruletree_string_hdr_t */
#define SB2_RULETREE_OBJECT_TYPE_OBJECTLIST	5	/* ruletree_objectlist_t */
#define SB2_RULETREE_OBJECT_TYPE_BINTREE	6	/* ruletree_bintree_t */
#define SB2_RULETREE_OBJECT_TYPE_INODESTAT	7	/* ruletree_inodestat_t */
#define SB2_RULETREE_OBJECT_TYPE_UINT32		8	/* ruletree_uint32_t */
#define SB2_RULETREE_OBJECT_TYPE_BOOLEAN	9	/* also ruletree_uint32_t */
#define SB2_RULETREE_OBJECT_TYPE_EXEC_PP_RULE	14	/* ruletree_exec_preprocessing_rule_t */
#define SB2_RULETREE_OBJECT_TYPE_EXEC_SEL_RULE	15	/* ruletree_exec_policy_selection_rule_t */

typedef struct ruletree_hdr_s {
	ruletree_object_hdr_t	rtree_hdr_objhdr;

	uint32_t		rtree_version;

	ruletree_object_offset_t	rtree_hdr_root_catalog;

	uint32_t		rtree_file_size;
	uint32_t		rtree_max_size;			/* used when mmap'ing */
	uint32_t		rtree_min_mmap_addr;		/* for clients */
	uint32_t		rtree_min_client_socket_fd;	/* for clients */
} ruletree_hdr_t;

#define RULE_TREE_VERSION	5

/* catalogs are lists of name+value pairs
 * (the value can be a rule, string, or another catalog).
 * Catalogs are a bit like directories, except
 * that the same name can appear multiple times in the catalog.
 * These are implemented as one-way linked lists.
*/
typedef struct ruletree_catalog_entry_s {
	ruletree_object_hdr_t	rtree_hdr_objhdr;

	ruletree_object_offset_t	rtree_cat_name_offs;
	ruletree_object_offset_t	rtree_cat_value_offs;

	ruletree_object_offset_t	rtree_cat_next_entry_offs;
} ruletree_catalog_entry_t;

typedef struct ruletree_fsrule_s {
	ruletree_object_hdr_t		rtree_fsr_objhdr;

	ruletree_object_offset_t	rtree_fsr_name_offs;

	uint32_t			rtree_fsr_selector_type;
	ruletree_object_offset_t	rtree_fsr_selector_offs;

	uint32_t			rtree_fsr_action_type;
	ruletree_object_offset_t	rtree_fsr_action_offs;
	ruletree_object_offset_t	rtree_fsr_rule_list_link;

	uint32_t			rtree_fsr_condition_type;
	ruletree_object_offset_t	rtree_fsr_condition_offs;

	uint32_t			rtree_fsr_flags;
        ruletree_object_offset_t	rtree_fsr_binary_name;
        uint32_t			rtree_fsr_func_class;
        ruletree_object_offset_t	rtree_fsr_exec_policy_name;

} ruletree_fsrule_t;

typedef struct ruletree_exec_preprocessing_rule_s {
	ruletree_object_hdr_t		rtree_xpr_objhdr;

	ruletree_object_offset_t	rtree_xpr_binary_name_offs;
        ruletree_object_offset_t	rtree_xpr_path_prefixes_table_offs;
        ruletree_object_offset_t	rtree_xpr_add_head_table_offs;
        ruletree_object_offset_t	rtree_xpr_add_options_table_offs;
        ruletree_object_offset_t	rtree_xpr_add_tail_table_offs;
        ruletree_object_offset_t	rtree_xpr_remove_table_offs;
        ruletree_object_offset_t	rtree_xpr_new_filename_offs;
        uint32_t			rtree_xpr_disable_mapping;
} ruletree_exec_preprocessing_rule_t;

typedef struct ruletree_exec_policy_selection_rule_s {
	ruletree_object_hdr_t		rtree_xps_objhdr;

	uint32_t			rtree_xps_type;
	uint32_t			rtree_xps_flags;
        ruletree_object_offset_t	rtree_xps_selector_offs;
        ruletree_object_offset_t	rtree_xps_exec_policy_name_offs;
} ruletree_exec_policy_selection_rule_t;

typedef struct ruletree_bintree_s {
	ruletree_object_hdr_t		rtree_bt_objhdr;
	uint64_t			rtree_bt_key1;
	uint64_t			rtree_bt_key2;
	ruletree_object_offset_t	rtree_bt_value;
	ruletree_object_offset_t	rtree_bt_link_less;
	ruletree_object_offset_t	rtree_bt_link_more;
} ruletree_bintree_t;

typedef struct {
	uint64_t	inodesimu_dev;     /* device containing it; used as key */
	uint64_t	inodesimu_ino;     /* inode number; used as key */

	uint64_t	inodesimu_rdev;	/* device id if special file */
	uint32_t	inodesimu_devmode; /* S_IFCHR or S_IFBLK if special file */

	uint32_t   	inodesimu_uid;	/* simulated UID */
	uint32_t   	inodesimu_gid;	/* simulated GID */
	uint32_t	inodesimu_mode;	/* simulated protection bits (Trwxrwxrwx) */
	uint32_t	inodesimu_suidsgid;	/* simulated SUID/SGID bits */

	uint32_t	inodesimu_active_fields;	/* bit mask (RULETREE_INODESTAT_SIM_*) */
} inodesimu_t;

typedef struct ruletree_inodestat_s {
	ruletree_object_hdr_t	rtree_inode_objhdr;

	inodesimu_t		rtree_inode_simu;
} ruletree_inodestat_t;

/* bit mask simulated_fields: */
#define RULETREE_INODESTAT_SIM_UID	0x1	/* set when UID simulation is active */
#define RULETREE_INODESTAT_SIM_GID	0x2	/* set when GID simulation is active */
#define RULETREE_INODESTAT_SIM_MODE	0x4	/* set when mode simulation is active */
#define RULETREE_INODESTAT_SIM_DEVNODE	0x8	/* set when simulating a blk/chr device */
#define RULETREE_INODESTAT_SIM_SUIDSGID	0x10	/* set when SUID/SGID simulation is active */

/* the string header structure is followed by the string itself. */
typedef struct ruletree_string_hdr_s {
	ruletree_object_hdr_t	rtree_str_objhdr;

	uint32_t	rtree_str_size;
} ruletree_string_hdr_t;

/* the object list structure is followed by the list itself
 * (an array of ruletree_object_offset_t) */
typedef struct ruletree_objectlist_s {
	ruletree_object_hdr_t	rtree_olist_objhdr;

	uint32_t	rtree_olist_size;
} ruletree_objectlist_t;

/* An unsigned integer, 32 bits. */
/* Also used to store booleans. */
typedef struct ruletree_uint32_s {
	ruletree_object_hdr_t	rtree_str_objhdr;

	uint32_t	rtree_uint32;
} ruletree_uint32_t;

/* the three "usual selectors", used in normal rules */
#define SB2_RULETREE_FSRULE_SELECTOR_PATH		101
#define SB2_RULETREE_FSRULE_SELECTOR_PREFIX		102
#define SB2_RULETREE_FSRULE_SELECTOR_DIR		103

#define SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE	200 /* FIXME. To be removed.*/
#define SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH	201
#define SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH	202
#define SB2_RULETREE_FSRULE_ACTION_MAP_TO		210
#define SB2_RULETREE_FSRULE_ACTION_REPLACE_BY		211
#define SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR 212
#define SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR 213
#define SB2_RULETREE_FSRULE_ACTION_SET_PATH		214
#define SB2_RULETREE_FSRULE_ACTION_CONDITIONAL_ACTIONS	220
#define SB2_RULETREE_FSRULE_ACTION_SUBTREE		230
#define SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_MAP_TO	245
#define SB2_RULETREE_FSRULE_ACTION_IF_EXISTS_THEN_REPLACE_BY	246
#define SB2_RULETREE_FSRULE_ACTION_PROCFS		250	/* /proc */
#define SB2_RULETREE_FSRULE_ACTION_UNION_DIR		251

/* auxiliar conditions */ 
#define SB2_RULETREE_FSRULE_CONDITION_IF_ACTIVE_EXEC_POLICY_IS 301
#define SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_IGNORE_IS_ACTIVE 302
#define SB2_RULETREE_FSRULE_CONDITION_IF_REDIRECT_FORCE_IS_ACTIVE 303
#define SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_NOT_EMPTY 304
#define SB2_RULETREE_FSRULE_CONDITION_IF_ENV_VAR_IS_EMPTY 305

/* ----------- rule_tree.c: ----------- */
extern int ruletree_to_memory(void); /* 0 if ok, negative if rule tree is not available. */

extern size_t ruletree_get_file_size(void);

extern int ruletree_get_min_client_socket_fd(void);

extern ruletree_object_offset_t append_struct_to_ruletree_file(void *ptr, size_t size, uint32_t type);

extern int link_ruletree_fsrules(ruletree_object_offset_t rule1_location, ruletree_object_offset_t rule2_location);
extern int set_ruletree_fsrules(const char *modename, const char *rules_name, int loc);

extern char *ruletree_reverse_path(
	const char *modename, const char *binary_name,
        const char *func_name, const char *full_path);

extern int create_ruletree_file(const char *ruletree_path,
	uint32_t max_size, uint32_t min_mmap_addr, int min_client_socket_fd);
extern int attach_ruletree(const char *ruletree_path, int keep_open);

extern void *offset_to_ruletree_object_ptr(ruletree_object_offset_t offs,
	uint32_t required_type);
extern const char *offset_to_ruletree_string_ptr(
	ruletree_object_offset_t offs, uint32_t *lenp);
extern ruletree_fsrule_t *offset_to_ruletree_fsrule_ptr(int loc);

extern ruletree_exec_preprocessing_rule_t *offset_to_exec_preprocessing_rule_ptr(int loc);

/* strings */
extern ruletree_object_offset_t append_string_to_ruletree_file(const char *str);

/* ints */
extern uint32_t *ruletree_get_pointer_to_uint32(ruletree_object_offset_t offs);
extern ruletree_object_offset_t append_uint32_to_ruletree_file(uint32_t initial_value);

/* booleans */
extern uint32_t *ruletree_get_pointer_to_boolean(ruletree_object_offset_t offs);
extern ruletree_object_offset_t append_boolean_to_ruletree_file(uint32_t initial_value);

/* lists */
extern ruletree_object_offset_t ruletree_objectlist_create_list(uint32_t size);
extern int ruletree_objectlist_set_item(ruletree_object_offset_t list_offs,
	uint32_t n, ruletree_object_offset_t value);
extern ruletree_object_offset_t ruletree_objectlist_get_item(
        ruletree_object_offset_t list_offs, uint32_t n);
extern uint32_t ruletree_objectlist_get_list_size(
        ruletree_object_offset_t list_offs);

/* catalogs */
extern ruletree_object_offset_t ruletree_catalog_get(
	const char *catalog_name, const char *object_name);
extern int ruletree_catalog_set(const char *catalog_name,
	const char *object_name, ruletree_object_offset_t value_offset);

extern ruletree_object_offset_t ruletree_catalog_vget(const char *namev[]);
extern int ruletree_catalog_vset(const char *namev[], ruletree_object_offset_t value_offset);

extern const char *ruletree_catalog_get_string(
	const char *catalog_name, const char *object_name);
extern uint32_t *ruletree_catalog_get_uint32_ptr(
	const char *catalog_name, const char *object_name);
extern uint32_t *ruletree_catalog_get_boolean_ptr(
	const char *catalog_name, const char *object_name);

extern ruletree_object_offset_t	ruletree_catalog_find_value_from_catalog(
	ruletree_object_offset_t first_catalog_entry_offs, const char *name);

/* inodestats */
typedef struct {
	uint64_t	rfh_dev;     /* device containing it; used as key */
	uint64_t	rfh_ino;     /* inode number; used as key */

	/* bintree node offset, if known */
	ruletree_object_offset_t	rfh_offs;

	/* next two fields are filled by ruletree_find_inodestat(),
	 * and used by ruletree_set_inodestat() */
	ruletree_object_offset_t        rfh_last_visited_node;
	int				rfh_last_result;
} ruletree_inodestat_handle_t;

#define ruletree_clear_inodestat_handle(p) \
	do {memset((p),0,sizeof(ruletree_inodestat_handle_t));} while(0)

#define ruletree_init_inodestat_handle(p, dev, ino) \
	do {ruletree_clear_inodestat_handle((p)); \
	    (p)->rfh_dev = (dev); (p)->rfh_ino = (ino); \
	} while(0)

extern int ruletree_find_inodestat(
	ruletree_inodestat_handle_t	*handle,
        inodesimu_t                      *istat_struct);

extern ruletree_object_offset_t ruletree_set_inodestat(
	ruletree_inodestat_handle_t	*handle,
        inodesimu_t      		*istat_struct);

/* ------------ fs mapping rule maintenance routines ------------ */
extern ruletree_object_offset_t add_rule_to_ruletree(
	const char *name, int selector_type, const char *selector,
	int action_type, const char *action_str,
	int condition_type, const char *condition_str,
	ruletree_object_offset_t rule_list_link,
	int flags, const char *binary_name,
        int func_class, const char *exec_policy_name);

/* ------------ exec rule maintenance routines ------------ */
ruletree_object_offset_t add_exec_preprocessing_rule_to_ruletree(
        const char      *binary_name,
        ruletree_object_offset_t path_prefixes_table_offs,
        ruletree_object_offset_t add_head_table_offs,
        ruletree_object_offset_t add_options_table_offs,
        ruletree_object_offset_t add_tail_table_offs,
        ruletree_object_offset_t remove_table_offs,
        const char *new_filename,
        int disable_mapping);

ruletree_object_offset_t add_exec_policy_selection_rule_to_ruletree(
	uint32_t	ruletype,
        const char      *selector,
        const char      *exec_policy_name,
	uint32_t	flags);

/* ------------ rule_tree_utils.c: ------------ */

extern void inc_vperm_num_active_inodestats(void);
extern void dec_vperm_num_active_inodestats(void);
extern uint32_t get_vperm_num_active_inodestats(void);

#endif /* SB2_RULETREE_H__ */
