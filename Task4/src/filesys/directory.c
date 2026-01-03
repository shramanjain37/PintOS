#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

static bool
dir_is_empty (struct dir *dir)
{
  struct inode *inode = dir_get_inode (dir);
  struct dir_entry e;
  off_t ofs;
  bool is_empty = true;

  for (ofs = sizeof e; inode_read_at (inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
    is_empty = is_empty && !e.in_use;
    if (!is_empty) break;
  }
  return is_empty;
}

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  if (sector == ROOT_DIR_SECTOR) {
    struct inode *root_dir;
    struct dir_entry e;

    if (inode_create (sector, entry_cnt * sizeof (struct dir_entry), true) == false)
      return false;
    root_dir = inode_open (sector);

    e.inode_sector = sector;
    strlcpy (e.name, "..", 3);
    e.in_use = true;
    
    bool success = inode_write_at (root_dir, &e, sizeof e, 0) == sizeof e;

    inode_close (root_dir);
    return success;
  }
  else
    return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = sizeof (struct dir_entry);
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (!strcmp (name, ".")) {
    *inode = inode_reopen (dir->inode);
  }
  else if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e)
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)) {
    goto done;
  }

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  bool is_opened = inode_is_opened (inode);
  if (inode == NULL ||
      inode_get_inumber (inode) == ROOT_DIR_SECTOR)
    goto done;


  if (inode_is_dir (inode)) {
    if (is_opened) {
      goto done;
    }

    struct dir *r_dir = dir_open (inode);
    if (!dir_is_empty (r_dir)) {
      dir_close (r_dir);
      goto done;
    }
    struct inode *cur_inode = dir_get_inode (thread_current ()->cur_dir);
    if (inode_get_inumber (cur_inode) == inode_get_inumber (inode)) {
      dir_close (r_dir);
      goto done;
    }
    dir_close (r_dir);
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

struct dir*
dir_open_dir (const char *dir)
{
  char *dir_cpy;
  char *save_ptr, *moveTo;
  const char *slash = "/";
  struct dir *cur_dir;

  dir_cpy = (char *)malloc(strlen (dir) + 1);
  strlcpy (dir_cpy, dir, strlen (dir) + 1);

  if (dir_cpy[0] == '/') {
    cur_dir = dir_open_root ();
  }
  else {
    if (thread_current ()->cur_dir == NULL)
      cur_dir = dir_open_root ();
    else
      cur_dir = dir_reopen (thread_current ()->cur_dir);
  }

  for (moveTo = strtok_r (dir_cpy, slash, &save_ptr);
       moveTo != NULL; moveTo = strtok_r (NULL, slash, &save_ptr)) {
    struct dir *next_dir;
    struct inode *next;
    bool success = dir_lookup (cur_dir, moveTo, &next);
    if (success) {
      next_dir = dir_open (next);
      dir_close (cur_dir);
      if (next_dir == NULL)
	return NULL;
      cur_dir = next_dir;
    }
    else {
      dir_close (cur_dir);
      return NULL;
    }
  }

  return cur_dir;
}

bool
dir_sub_create (block_sector_t sector, char *name, struct dir *prev_dir)
{
  ASSERT (sector != ROOT_DIR_SECTOR);

  struct inode *dir_inode;
  struct dir *dir;
  bool success;

  if (lookup (prev_dir, name, NULL, NULL))
    return false;

  success = dir_create (sector, 16);

  if (success) {
    if ((dir_inode = inode_open (sector)) != NULL) {
      if ((dir = dir_open (dir_inode)) != NULL) {
        success = dir_add (dir, "..", inode_get_inumber (prev_dir->inode))
      		  && dir_add (prev_dir, name, sector);
        dir_close (dir);
      }
      else {
        success = false;
        inode_close (dir_inode);
      }
    }
    else success = false;
  }

  return success;
}

bool
dir_parse (const char *dir, char base[NAME_MAX + 1], char name[NAME_MAX + 1])
{
  int len = strlen (dir) + 1;
  char *dir_cpy;
  char *last_slash = NULL;
 
  if (len == 1)
    return false;
 
  dir_cpy = (char *)calloc (1, len);
  strlcpy (dir_cpy, dir, len);

  last_slash = strrchr (dir_cpy, (int)'/');
  if (last_slash == NULL) {
    if (len > NAME_MAX + 1) {
      free (dir_cpy);
      return false;
    }
    strlcpy (base, ".", 2);
    strlcpy (name, dir, len);
  }
  else {
    *last_slash = '\0';
    int base_len = strlen (dir_cpy) + 1;
    *last_slash = '/';
    
    if (len - base_len > NAME_MAX + 1) {
      free (dir_cpy);
      return false;
    }
    strlcpy (base, dir_cpy, base_len + 1);
    strlcpy (name, last_slash + 1, len - base_len);
  }
  free (dir_cpy);
  return true;
}

bool
dir_lookup_by_sector (struct dir *dir, block_sector_t sector, char **name)
{
  struct dir_entry e;
  size_t ofs;
  struct inode *inode = dir_get_inode (dir);

  for (ofs = 0; inode_read_at (inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
    if (e.in_use && e.inode_sector == sector) {
      strlcpy (*name, e.name, strlen (e.name) + 1);
      return true;
    }
  }
  return false;
}
