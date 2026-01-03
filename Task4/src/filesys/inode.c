#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"  

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCKS 12
#define INDIRECT_SIZE 128
#define DOUBLY_INDIRECT_SIZE (1 << 14)

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes.  */
    block_sector_t direct[12];          /* Direct blocks. */
    block_sector_t indirect;            /* Singly indirect blocks. */
    block_sector_t doubly_indirect;     /* Doubly indirect blocks. */
    uint32_t is_dir;                    /* 1: directory; 0: file */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[111];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    bool is_dir;
  };

static bool inode_release (struct inode *inode);


static block_sector_t
inode_single_indirect (block_sector_t indirect, off_t index)
{
  ASSERT (0 <= index && index < 128);

  block_sector_t buf[128];

  buffer_cache_read (indirect, buf);
  return buf[index];
}

static block_sector_t
inode_doubly_indirect (block_sector_t doubly_indirect, off_t index)
{
  ASSERT (0 <= index && index < (1 << 14));

  block_sector_t buf[128];

  buffer_cache_read (doubly_indirect, buf);
  return inode_single_indirect (buf[index / 128], index % 128); 
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length) {
    off_t index = pos / BLOCK_SECTOR_SIZE;
    if (index < DIRECT_BLOCKS) {
      return inode->data.direct[index];
    }
    if (index < DIRECT_BLOCKS + INDIRECT_SIZE) {
      return inode_single_indirect (inode->data.indirect, index - DIRECT_BLOCKS);
    }
    return inode_doubly_indirect (inode->data.doubly_indirect,
                                  index - DIRECT_BLOCKS - INDIRECT_SIZE);
  }
  else
    return -1;
}

static bool
inode_extend (struct inode_disk *disk_inode, size_t sectors)
{
  static char zeros[BLOCK_SECTOR_SIZE];
  
  size_t num_sectors = sectors < DIRECT_BLOCKS ? sectors : DIRECT_BLOCKS;
  for (size_t i = 0; i < num_sectors; i++) {
    if (disk_inode->direct[i] == 0) {
      free_map_allocate (1, &disk_inode->direct[i]);
      buffer_cache_write (disk_inode->direct[i], zeros);
    }
  }
  sectors -= num_sectors;
  if (!sectors) return true;

  num_sectors = sectors < INDIRECT_SIZE ? sectors : INDIRECT_SIZE;
  if (disk_inode->indirect == 0) {
    free_map_allocate (1, &disk_inode->indirect);
    buffer_cache_write (disk_inode->indirect, zeros);
  }

  block_sector_t indirect_block[128];
  buffer_cache_read (disk_inode->indirect, indirect_block);

  for (size_t i = 0; i < num_sectors; i++) {
    if (indirect_block[i] == 0) {
      free_map_allocate (1, &indirect_block[i]);
      buffer_cache_write (indirect_block[i], zeros);
    }
  }
  buffer_cache_write (disk_inode->indirect, indirect_block);
  sectors -= num_sectors;
  if (!sectors) return true;

  num_sectors = sectors < DOUBLY_INDIRECT_SIZE ? sectors : DOUBLY_INDIRECT_SIZE;
  if (disk_inode->doubly_indirect == 0) {
    free_map_allocate (1, &disk_inode->doubly_indirect);
    buffer_cache_write (disk_inode->doubly_indirect, zeros);
  }
  
  block_sector_t doubly_indirect_block[128];
  buffer_cache_read (disk_inode->doubly_indirect, doubly_indirect_block);
  
  for (size_t i = 0; i < num_sectors / INDIRECT_SIZE + 1; i++) {
    if (doubly_indirect_block[i] == 0) {
      free_map_allocate (1, &doubly_indirect_block[i]);
      buffer_cache_write (doubly_indirect_block[i], zeros);
    }
    buffer_cache_read (doubly_indirect_block[i], indirect_block);
    size_t rot = (i == num_sectors / INDIRECT_SIZE) ? 
               (num_sectors % INDIRECT_SIZE) : INDIRECT_SIZE;
    for (size_t j = 0; j < rot; j++) {
      if (indirect_block[j] == 0) {
        free_map_allocate (1, &indirect_block[j]);
        buffer_cache_write (indirect_block[j], zeros);
      }
    }
    buffer_cache_write (doubly_indirect_block[i], indirect_block);
  }
  buffer_cache_write (disk_inode->doubly_indirect, doubly_indirect_block);
  sectors -= num_sectors;
  if (!sectors) return true;

  return false;
}

/* List of open inodes, so that opening a single inode twice
   returns the same struct inode. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL) {
    size_t sectors = bytes_to_sectors (length);
    disk_inode->magic = INODE_MAGIC;

    success = inode_extend (disk_inode, sectors);
    if (success) { 
      disk_inode->length = length;
      disk_inode->is_dir = is_dir ? 1 : 0;
      buffer_cache_write (sector, disk_inode);
    }
    free (disk_inode);
  }
  return success;
}

/* Reads an inode from SECTOR
   and returns a struct inode that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  buffer_cache_read (inode->sector, &inode->data);
  inode->is_dir = inode->data.is_dir == 1 ? true : false;
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          inode_release (inode);
        }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less than SIZE
   if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          buffer_cache_read (sector_idx, buffer + bytes_read);
        }
      else 
        {
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          buffer_cache_read (sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  free (bounce);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.

   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if (byte_to_sector (inode, offset + size - 1) == -1u) {
    static char zeros[BLOCK_SECTOR_SIZE];
    struct inode_disk *disk_inode = &inode->data;
    disk_inode->length = offset + size;
    inode_extend (disk_inode, bytes_to_sectors (disk_inode->length));
    buffer_cache_write (inode->sector, disk_inode);
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          buffer_cache_write (sector_idx, buffer + bytes_written);
        }
      else 
        {
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            buffer_cache_read (sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          buffer_cache_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  free (bounce);
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

static bool
inode_release (struct inode *inode)
{
  struct inode_disk *disk_inode = &inode->data;

  for (size_t i = 0; i < DIRECT_BLOCKS; i++) {
    if (disk_inode->direct[i] != 0) {
      free_map_release (disk_inode->direct[i], 1);
    }
  }

  if (disk_inode->indirect != 0) {
    block_sector_t indirect_block[128];
    buffer_cache_read (disk_inode->indirect, indirect_block);

    for (size_t i = 0; i < 128; i++) {
      if (indirect_block[i] != 0) {
        free_map_release (indirect_block[i], 1);
      }
    }
    free_map_release (disk_inode->indirect, 1);
  }

  if (disk_inode->doubly_indirect != 0) {
    block_sector_t doubly_indirect_block[128];
    buffer_cache_read (disk_inode->doubly_indirect, doubly_indirect_block);

    for (size_t i = 0; i < 128; i++) {
      if (doubly_indirect_block[i] != 0) {
        block_sector_t indirect_block[128];
        buffer_cache_read (doubly_indirect_block[i], indirect_block);
        
        for (size_t j = 0; j < 128; j++) {
          if (indirect_block[j] != 0) {
            free_map_release (indirect_block[j], 1);
          }
        }
        free_map_release (doubly_indirect_block[i], 1);
      }
    }
    free_map_release (disk_inode->doubly_indirect, 1);
  }
  return true;
}

bool
inode_is_dir (struct inode *inode)
{
  return inode->is_dir;
}

bool
inode_is_opened (struct inode *inode)
{
  return inode->open_cnt > 1;
}
