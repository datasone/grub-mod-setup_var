
#include <grub/types.h>
#include <grub/disk.h>

/* A HFS+ extent.  */
struct grub_hfsplus_extent
{
  /* The first block of a file on disk.  */
  grub_uint32_t start;
  /* The amount of blocks described by this extent.  */
  grub_uint32_t count;
} __attribute__ ((packed));

/* The descriptor of a fork.  */
struct grub_hfsplus_forkdata
{
  grub_uint64_t size;
  grub_uint32_t clumpsize;
  grub_uint32_t blocks;
  struct grub_hfsplus_extent extents[8];
} __attribute__ ((packed));

/* The HFS+ Volume Header.  */
struct grub_hfsplus_volheader
{
  grub_uint16_t magic;
  grub_uint16_t version;
  grub_uint32_t attributes;
  grub_uint8_t unused1[12];
  grub_uint32_t utime;
  grub_uint8_t unused2[16];
  grub_uint32_t blksize;
  grub_uint8_t unused3[60];
  grub_uint64_t num_serial;
  struct grub_hfsplus_forkdata allocations_file;
  struct grub_hfsplus_forkdata extents_file;
  struct grub_hfsplus_forkdata catalog_file;
  struct grub_hfsplus_forkdata attr_file;
  struct grub_hfsplus_forkdata startup_file;
} __attribute__ ((packed));

struct grub_hfsplus_compress_index
{
  grub_uint32_t start;
  grub_uint32_t size;
};

struct grub_hfsplus_file
{
  struct grub_hfsplus_data *data;
  struct grub_hfsplus_extent extents[8];
  struct grub_hfsplus_extent resource_extents[8];
  grub_uint64_t size;
  grub_uint64_t resource_size;
  grub_uint32_t fileid;
  grub_int32_t mtime;
  int compressed;
  char *cbuf;
  struct grub_hfsplus_compress_index *compress_index;
  grub_uint32_t cbuf_block;
  grub_uint32_t compress_index_size;
};

struct grub_hfsplus_btree
{
  grub_uint32_t root;
  grub_size_t nodesize;

  /* Catalog file node.  */
  struct grub_hfsplus_file file;
};

/* Information about a "mounted" HFS+ filesystem.  */
struct grub_hfsplus_data
{
  struct grub_hfsplus_volheader volheader;
  grub_disk_t disk;

  unsigned int log2blksize;

  struct grub_hfsplus_btree catalog_tree;
  struct grub_hfsplus_btree extoverflow_tree;
  struct grub_hfsplus_btree attr_tree;

  struct grub_hfsplus_file dirroot;
  struct grub_hfsplus_file opened_file;

  /* This is the offset into the physical disk for an embedded HFS+
     filesystem (one inside a plain HFS wrapper).  */
  grub_disk_addr_t embedded_offset;
  int case_sensitive;
};

/* Internal representation of a catalog key.  */
struct grub_hfsplus_catkey_internal
{
  grub_uint32_t parent;
  const grub_uint16_t *name;
  grub_size_t namelen;
};

/* Internal representation of an extent overflow key.  */
struct grub_hfsplus_extkey_internal
{
  grub_uint32_t fileid;
  grub_uint32_t start;
  grub_uint8_t type;
};

struct grub_hfsplus_attrkey
{
  grub_uint16_t keylen;
  grub_uint16_t unknown1[1];
  grub_uint32_t cnid;
  grub_uint16_t unknown2[2];
  grub_uint16_t namelen;
  grub_uint16_t name[0];
} __attribute__ ((packed));

struct grub_hfsplus_attrkey_internal
{
  grub_uint32_t cnid;
  const grub_uint16_t *name;
  grub_size_t namelen;
};

struct grub_hfsplus_key_internal
{
  union
  {
    struct grub_hfsplus_extkey_internal extkey;
    struct grub_hfsplus_catkey_internal catkey;
    struct grub_hfsplus_attrkey_internal attrkey;
  };
};

/* The on disk layout of a catalog key.  */
struct grub_hfsplus_catkey
{
  grub_uint16_t keylen;
  grub_uint32_t parent;
  grub_uint16_t namelen;
  grub_uint16_t name[30];
} __attribute__ ((packed));

/* The on disk layout of an extent overflow file key.  */
struct grub_hfsplus_extkey
{
  grub_uint16_t keylen;
  grub_uint8_t type;
  grub_uint8_t unused;
  grub_uint32_t fileid;
  grub_uint32_t start;
} __attribute__ ((packed));

struct grub_hfsplus_key
{
  union
  {
    struct grub_hfsplus_extkey extkey;
    struct grub_hfsplus_catkey catkey;
    struct grub_hfsplus_attrkey attrkey;
    grub_uint16_t keylen;
  };
} __attribute__ ((packed));

struct grub_hfsplus_btnode
{
  grub_uint32_t next;
  grub_uint32_t prev;
  grub_int8_t type;
  grub_uint8_t height;
  grub_uint16_t count;
  grub_uint16_t unused;
} __attribute__ ((packed));

/* Return the offset of the record with the index INDEX, in the node
   NODE which is part of the B+ tree BTREE.  */
static inline grub_off_t
grub_hfsplus_btree_recoffset (struct grub_hfsplus_btree *btree,
			   struct grub_hfsplus_btnode *node, int index)
{
  char *cnode = (char *) node;
  void *recptr;
  recptr = (&cnode[btree->nodesize - index * sizeof (grub_uint16_t) - 2]);
  return grub_be_to_cpu16 (grub_get_unaligned16 (recptr));
}

/* Return a pointer to the record with the index INDEX, in the node
   NODE which is part of the B+ tree BTREE.  */
static inline struct grub_hfsplus_key *
grub_hfsplus_btree_recptr (struct grub_hfsplus_btree *btree,
			   struct grub_hfsplus_btnode *node, int index)
{
  char *cnode = (char *) node;
  grub_off_t offset;
  offset = grub_hfsplus_btree_recoffset (btree, node, index);
  return (struct grub_hfsplus_key *) &cnode[offset];
}

extern grub_err_t (*grub_hfsplus_open_compressed) (struct grub_hfsplus_file *node);
extern grub_ssize_t (*grub_hfsplus_read_compressed) (struct grub_hfsplus_file *node,
						     grub_off_t pos,
						     grub_size_t len,
						     char *buf);

grub_ssize_t
grub_hfsplus_read_file (struct grub_hfsplus_file *node,
			grub_disk_read_hook_t read_hook, void *read_hook_data,
			grub_off_t pos, grub_size_t len, char *buf);
grub_err_t
grub_hfsplus_btree_search (struct grub_hfsplus_btree *btree,
			   struct grub_hfsplus_key_internal *key,
			   int (*compare_keys) (struct grub_hfsplus_key *keya,
						struct grub_hfsplus_key_internal *keyb),
			   struct grub_hfsplus_btnode **matchnode, 
			   grub_off_t *keyoffset);
