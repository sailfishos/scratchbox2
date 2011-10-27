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

typedef struct ruletree_hdr_s {
	ruletree_object_hdr_t	rtree_hdr_objhdr;

	uint32_t		rtree_version;

	ruletree_object_offset_t	rtree_hdr_root_catalog;
} ruletree_hdr_t;

#define RULE_TREE_VERSION	3

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

/* the three "usual selectors", used in normal rules */
#define SB2_RULETREE_FSRULE_SELECTOR_PATH		101
#define SB2_RULETREE_FSRULE_SELECTOR_PREFIX		102
#define SB2_RULETREE_FSRULE_SELECTOR_DIR		103

#define SB2_RULETREE_FSRULE_ACTION_FALLBACK_TO_OLD_MAPPING_ENGINE	200
#define SB2_RULETREE_FSRULE_ACTION_USE_ORIG_PATH	201
#define SB2_RULETREE_FSRULE_ACTION_FORCE_ORIG_PATH	202
#define SB2_RULETREE_FSRULE_ACTION_MAP_TO		210
#define SB2_RULETREE_FSRULE_ACTION_REPLACE_BY		211
#define SB2_RULETREE_FSRULE_ACTION_MAP_TO_VALUE_OF_ENV_VAR 212
#define SB2_RULETREE_FSRULE_ACTION_REPLACE_BY_VALUE_OF_ENV_VAR 213
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

extern ruletree_object_offset_t append_struct_to_ruletree_file(void *ptr, size_t size, uint32_t type);

extern int link_ruletree_fsrules(ruletree_object_offset_t rule1_location, ruletree_object_offset_t rule2_location);
extern int set_ruletree_fsrules(const char *modename, const char *rules_name, int loc);

extern char *ruletree_reverse_path(
	const char *modename, const char *binary_name,
        const char *func_name, const char *full_path);

extern int attach_ruletree(const char *ruletree_path,
	int create_if_it_doesnt_exist, int keep_open);

extern void *offset_to_ruletree_object_ptr(ruletree_object_offset_t offs, uint32_t required_type);
extern const char *offset_to_ruletree_string_ptr(ruletree_object_offset_t offs);
extern ruletree_fsrule_t *offset_to_ruletree_fsrule_ptr(int loc);

/* strings */
extern ruletree_object_offset_t append_string_to_ruletree_file(const char *str);

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

/* ------------ rule_tree_luaif.c: ------------ */
extern int lua_bind_ruletree_functions(lua_State *l);

extern ruletree_object_offset_t add_rule_to_ruletree(
	const char *name, int selector_type, const char *selector,
	int action_type, const char *action_str,
	int condition_type, const char *condition_str,
	ruletree_object_offset_t rule_list_link,
	int flags, const char *binary_name,
        int func_class, const char *exec_policy_name);

