//
// Support functions for system calls that involve file descriptors.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE){
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
// addr is a user virtual address, pointing to a struct stat.
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

// Read from file f.
// addr is a user virtual address.
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  if(f->readable == 0)
    return -1;

  if(f->type == FD_PIPE){
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    ilock(f->ip);
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r;
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

// Write to file f.
// addr is a user virtual address.
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  if(f->writable == 0)
    return -1;

  if(f->type == FD_PIPE){
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // write a few blocks at a time to avoid exceeding
    // the maximum log transaction size, including
    // i-node, indirect block, allocation blocks,
    // and 2 blocks of slop for non-aligned writes.
    // this really belongs lower down, since writei()
    // might be writing a device like the console.
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r != n1){
        // error from writei
        break;
      }
      i += r;
    }
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

int
encrypt(int fd, uint8 key)
{
  // Check if file descriptor is valid
  if (fd < 0)
    return -1;

  // Retrieve the file pointer associated with the file descriptor
  struct file *f = myproc()->ofile[fd];

  // Check if the file pointer is valid
  if (f == 0) 
    return -1;
  ilock(f->ip);

  // Check if the file is already encrypted
  if (f->ip->encrypted) {
    iunlock(f->ip);
    return -1;
  }

  // Set the encrypted flag to prevent re-encryption
  f->ip->encrypted = 1;

  // Buffer to hold read bytes
  char buf[512];
  //printf("enc111r size: %d\n", f->ip->size);

  // Loop to read and encrypt the file byte-by-byte
  int n = 1, off=0;
  for (off = 0; off < f->ip->size ; off += n) 
  {
      n = readi(f->ip, 0, (uint64)&buf, off, sizeof(buf));
    // XOR encryption
        for (int i = 0; i < n; i++) 
        {
            // XOR each byte with the key
            buf[i] = buf[i] ^ key; // XOR encryption
        }
      // Write the encrypted bytes back to the file
      begin_op();
      writei(f->ip, 0, (uint64)&buf, off, n);
      end_op();
    }
    begin_op();
      iupdate(f->ip);
      end_op();
    iunlock(f->ip);
    return 0;
}
  
  

  int 
  decrypt(int fd, uint8 key) 
  {
    // Check if file descriptor is valid
    if (fd < 0)
      return -1;

    // Retrieve the file pointer associated with the file descriptor
    struct file *f = myproc()->ofile[fd];

    // Check if the file pointer is valid
    if (f == 0)
      return -1;
    ilock(f->ip);

    // Check if the file is not encrypted
    if (!f->ip->encrypted)
    {
      iunlock(f->ip);
      return -1;
    }

    // Clear the encrypted flag to allow decryption
     f->ip->encrypted = 0;

    // Buffer to hold read bytes
    char buf[512];

    // Loop to read and decrypt the file byte-by-byte
    int n = 1, off = 0;
    for (off = 0; off < f->ip->size; off += n) 
    {
        n = readi(f->ip, 0, (uint64)&buf, off, sizeof(buf));
        // XOR decryption
        for (int i = 0; i < n; i++) 
        {
            // XOR each byte with the key to decrypt
            buf[i] = buf[i] ^ key; // XOR decryption
        }
        // Write the decrypted bytes back to the file
        begin_op();
        writei(f->ip, 0, (uint64)&buf, off, n);
        end_op();
    }
    begin_op();
    iupdate(f->ip);
    end_op();
    iunlock(f->ip);
    return 0;
}



