// 2020/OS/hw4/B511021/김민수
// Simple FIle System
// Student Name : 김민수
// Student Number : B511021
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

#include <errno.h>
#include <err.h>

#define BLOCKSIZE  512

#ifndef EINTR
#define EINTR 0
#endif

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void init_dir(struct sfs_dir *d){
	int i;
	for(i = 0; i < SFS_DENTRYPERBLOCK; i++){
		d[i].sfd_ino = SFS_NOINO;
		strncpy(d[i].sfd_name, "", SFS_NAMELEN);
	}
}

u_int32_t findBlock(){
	int i, j, k;
	u_int8_t bitmapArray[SFS_BLOCKSIZE];
	for(i = 0; i < SFS_BITBLOCKS(spb.sp_nblocks); i++){
		disk_read(bitmapArray, SFS_MAP_LOCATION + i);
		for(j = 0; j < SFS_BLOCKSIZE; j++){
			if(bitmapArray[j] != 0xff){
				for(k = 0; k < 8; k++){
					if(!BIT_CHECK(bitmapArray[j], k)){
						BIT_SET(bitmapArray[j], k);
						disk_write(bitmapArray, SFS_MAP_LOCATION + i);
						//printf("bit: %d\n", i*512+j*8+k);
						return  (i * 512 * 8 + j * 8 + k);
					}
				}
			}
		}
	}
	if(i == SFS_BITBLOCKS(spb.sp_nblocks))
		return 0;
}

void bitSet(u_int32_t num){
	u_int32_t i, j, k;
	u_int8_t bitmapArray[SFS_BLOCKSIZE];
	for(i = 0; i < SFS_BITBLOCKS(spb.sp_nblocks); i++){
		disk_read(bitmapArray, SFS_MAP_LOCATION + i);
		for(j = 0; j < SFS_BLOCKSIZE; j++){
			for(k = 0; k < 8; k++){
				if( num == i * 512 * 8 + j * 8 + k){
					BIT_CLEAR(bitmapArray[j], k);
					disk_write(bitmapArray, SFS_MAP_LOCATION + i);
					return;
				}
			}
		}
	}
}

static int fd1=-1;

void
disk_open1(const char *path)
{
	assert(fd1<0);
	fd1 = open(path, O_RDWR);

	if (fd1<0) {
		err(1, "%s", path);
	}
}

u_int32_t
disk_blocksize1(void)
{
	assert(fd1>=0);
	return BLOCKSIZE;
}

void
disk_write1(const void *data, u_int32_t block, u_int32_t x)
{
	const char *cdata = data;
	u_int32_t tot=0;
	int len;
	assert(fd1>=0);

	if (lseek(fd1, block*BLOCKSIZE, SEEK_SET)<0) {
		err(1, "lseek");
	}
	if( x <512){
		while (tot < x){
			len = write(fd1, cdata + tot, x - tot);
			if(len < 0) {
				if(errno=EINTR || errno==EAGAIN) {
					continue;
				}
				err(1, "write");
			}
			if(len==0){
				err(1, "write returned 0?");
			}
			tot += len;
		}
	}
	else{
	while (tot < BLOCKSIZE) {
		len = write(fd1, cdata + tot, BLOCKSIZE - tot);
		if (len < 0) {
			if (errno==EINTR || errno==EAGAIN) {
				continue;
			}
			err(1, "write");
		}
		if (len==0) {
			err(1, "write returned 0?");
		}
		tot += len;
	}
	}
}

u_int32_t
disk_read1(void *data, u_int32_t block)
{
	char *cdata = data;
	u_int32_t tot=0;
	int len;

	assert(fd1>=0);

	if (lseek(fd1, block*BLOCKSIZE, SEEK_SET)<0) {
		err(1, "lseek");
	}

	while (tot < BLOCKSIZE) {
		len = read(fd1, cdata + tot, BLOCKSIZE - tot);
		if (len < 0) {
			if (errno==EINTR || errno==EAGAIN) {
				continue;
			}
			err(1, "read");
		}
		if (len==0) {
			return tot;
		}
		tot += len;
	}
	return tot;
}

void
disk_close1(void)
{
	assert(fd1>=0);
	if (close(fd1)) {
		err(1, "close");
	}
	fd1 = -1;
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}
}

void sfs_touch(const char* path)
{
	/*
	//skeleton implementation

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined
	
	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	//block access
	disk_read( sd, si.sfi_direct[0] );

	//allocate new block
	int newbie_ino = 6;

	sd[2].sfd_ino = newbie_ino;
	strncpy( sd[2].sfd_name, path, SFS_NAMELEN );

	disk_write( sd, si.sfi_direct[0] );

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;

	disk_write( &newbie, newbie_ino );
	*/

	struct sfs_inode si, si2;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd2[SFS_DENTRYPERBLOCK];
	int i, j;
	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] == 0){
			if( (si.sfi_direct[i] = findBlock()) == 0){
				error_message("touch", path, -4);
				return;
			}
			init_dir(sd2);
			if((sd2[0].sfd_ino = findBlock()) == 0){
				error_message("touch", path, -4);
				return;
			}
			strncpy(sd2[0].sfd_name, path, SFS_NAMELEN);
			bzero(&si2, SFS_BLOCKSIZE);
			si.sfi_size += 64;
			si2.sfi_size = 0;
			si2.sfi_type = SFS_TYPE_FILE;
			disk_write(&si, sd_cwd.sfd_ino);
			disk_write(&si2, sd2[0].sfd_ino);
			disk_write(sd2, si.sfi_direct[i]);
			return;
		}
		disk_read(sd, si.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if(sd[j].sfd_ino == 0){
				if((sd[j].sfd_ino = findBlock()) == 0){
					error_message("touch", path, -4);
					return;
				}
				strncpy(sd[j].sfd_name, path, SFS_NAMELEN);
				bzero(&si2, SFS_BLOCKSIZE);
				si.sfi_size += 64;
				si2.sfi_size = 0;
				si2.sfi_type = SFS_TYPE_FILE;
				disk_write(&si, sd_cwd.sfd_ino);
				disk_write(&si2, sd[j].sfd_ino);
				disk_write(sd, si.sfi_direct[i]); 
				return;
			}
			if(!strcmp(sd[j].sfd_name, path)){
				error_message("touch", path, -6);
				return;
			}
		}
	}
	if(i == SFS_NDIRECT){
		error_message("touch", path, -3);
		return;
	}
}

void sfs_cd(const char* path)
{
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];
	struct sfs_inode si, si2;
	int i, j;
	if(path == NULL){
		sd_cwd.sfd_ino = 1;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
	}
	else{
		disk_read(&si, sd_cwd.sfd_ino);
		for(i = 0; i < SFS_NDIRECT; i++){
			if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){
					//error_message("cd", path, -1);
					//return;
				}
				else{
				 if(!strcmp(sd[j].sfd_name, path) && sd[j].sfd_ino != 0){
					 disk_read(&si2, sd[j].sfd_ino);
					 if(si2.sfi_type == 1){
						 error_message("cd", path, -5);
						 return;
					 }
					 if(si2.sfi_type == 2){
					 	sd_cwd.sfd_ino = sd[j].sfd_ino;
					 	strncpy( sd_cwd.sfd_name, path, SFS_NAMELEN );
						return;
					 }
				 }
				}
			}
			}
		}
		error_message("cd", path, -1);
		return;
	}
}

void sfs_ls(const char* path)
{
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd2[SFS_DENTRYPERBLOCK];
	struct sfs_inode si, si2, si3, si4;
	int i, j, k, l;
	int count = 0;
	if(path == NULL){
		disk_read(&si, sd_cwd.sfd_ino);
		for(i = 0; i< SFS_NDIRECT; i++){
			if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){
					//error_message("ls", path, -1);
					//return;
				}
				else{
					disk_read(&si2, sd[j].sfd_ino);
					if(si2.sfi_type == 1){
						printf("%s\t", sd[j].sfd_name);
						count++;
					}
					else if(si2.sfi_type == 2){
						printf("%s/\t", sd[j].sfd_name);
						count++;
					}
				}
			}
			}
		}
		if(si.sfi_indirect != 0){
			disk_read(&si, si.sfi_indirect);
		for(i = 0; i< SFS_NDIRECT; i++){
			if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){
					//error_message("ls", path, -1);
					//return;
				}
				else{
					disk_read(&si2, sd[j].sfd_ino);
					if(si2.sfi_type == 1){
						printf("%s\t", sd[j].sfd_name);
						count++;
					}
					else if(si2.sfi_type == 2){
						printf("%s/\t", sd[j].sfd_name);
						count++;
					}
				}
			}
			}
		}
			}
		if(i == SFS_NDIRECT)
			printf("\n");
		return;
	}
	else{
		disk_read(&si, sd_cwd.sfd_ino);
		for(i = 0; i < SFS_NDIRECT; i++){
			if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){}
				else{
					if(!strcmp(sd[j].sfd_name, path)){
						disk_read(&si2, sd[j].sfd_ino);
						if(si2.sfi_type == 1){
							printf("%s\n",sd[j].sfd_name);
							return;
						}
						if(si2.sfi_type == 2){
							for(k = 0; k < SFS_NDIRECT; k++){
								if(si2.sfi_direct[k] != 0){
								disk_read(sd2, si2.sfi_direct[k]);
								for(l = 0; l < SFS_DENTRYPERBLOCK; l++){
									if(sd2[l].sfd_ino == 0){
										//error_message("ls", path, -1);
										//return;
									}
									else{	
										disk_read(&si3, sd2[l].sfd_ino);
										if(si3.sfi_type == 1){
											printf("%s\t", sd2[l].sfd_name);
											count++;
										}
										else if(si3.sfi_type == 2){
											printf("%s/\t", sd2[l].sfd_name);
											count++;
										}
									}
								}
								}
							}
							if(k == SFS_NDIRECT){
								printf("\n");
								return;
							}
						}
					}
				}
			}
			}
		}
		if(si.sfi_indirect != 0){
			disk_read(&si, si.sfi_indirect);
		for(i = 0; i < SFS_NDIRECT; i++){
			if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){}
				else{
					if(!strcmp(sd[j].sfd_name, path)){
						disk_read(&si2, sd[j].sfd_ino);
						if(si2.sfi_type == 1){
							printf("%s\n",sd[j].sfd_name);
							return;
						}
						if(si2.sfi_type == 2){
							for(k = 0; k < SFS_NDIRECT; k++){
								if(si2.sfi_direct[k] != 0){
								disk_read(sd2, si2.sfi_direct[k]);
								for(l = 0; l < SFS_DENTRYPERBLOCK; l++){
									if(sd2[l].sfd_ino == 0){
										//error_message("ls", path, -1);
										//return;
									}
									else{	
										disk_read(&si3, sd2[l].sfd_ino);
										if(si3.sfi_type == 1){
											printf("%s\t", sd2[l].sfd_name);
											count++;
										}
										else if(si3.sfi_type == 2){
											printf("%s/\t", sd2[l].sfd_name);
											count++;
										}
									}
								}
								}
							}
							if(k == SFS_NDIRECT){
								printf("\n");
								return;
							}
						}
					}
				}
			}
			}
		}
		}for(i = 0; i < SFS_NDIRECT; i++){
			if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){}
				else{
					if(!strcmp(sd[j].sfd_name, path)){
						disk_read(&si2, sd[j].sfd_ino);
						if(si2.sfi_type == 1){
							printf("%s\n",sd[j].sfd_name);
							return;
						}
						if(si2.sfi_type == 2){
							for(k = 0; k < SFS_NDIRECT; k++){
								disk_read(sd2, si2.sfi_direct[k]);
								for(l = 0; l < SFS_DENTRYPERBLOCK; l++){
									if(sd2[l].sfd_ino == 0){
										//error_message("ls", path, -1);
										//return;
									}
									else{	
										disk_read(&si3, sd2[l].sfd_ino);
										if(si3.sfi_type == 1){
											printf("%s\t", sd2[l].sfd_name);
											count++;
										}
										else if(si3.sfi_type == 2){
											printf("%s/\t", sd2[l].sfd_name);
											count++;
										}
									}
								}
							}
							if(k == SFS_NDIRECT){
								printf("\n");
								return;
							}
						}
					}
				}
			}
			}
		}
		if(i == SFS_NDIRECT){
			error_message("ls", path, -1);
			return;
		}
	}
}

void sfs_mkdir(const char* org_path) 
{
	struct sfs_inode si, si2;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd2[SFS_DENTRYPERBLOCK], sd3[SFS_DENTRYPERBLOCK];
	int i, j;
	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] == 0){
			if( (si.sfi_direct[i] = findBlock()) == 0){
				error_message("mkdir", org_path, -4);
				return;
			}
			init_dir(sd2);
			if((sd2[0].sfd_ino = findBlock()) == 0){
				error_message("mkdir", org_path, -4);
				return;
			}
			
			strncpy(sd2[0].sfd_name, org_path, SFS_NAMELEN);
			bzero(&si2, SFS_BLOCKSIZE);
			si.sfi_size += 64;
			si2.sfi_size = 128;
			si2.sfi_type = SFS_TYPE_DIR;
			if((si2.sfi_direct[0] = findBlock()) == 0){
				error_message("mkdir", org_path, -4);
				return;
			}
			init_dir(sd3);
			sd3[0].sfd_ino = sd2[0].sfd_ino;
			sd3[1].sfd_ino = sd_cwd.sfd_ino;
			strncpy(sd3[0].sfd_name, ".", SFS_NAMELEN);
			strncpy(sd3[1].sfd_name, "..", SFS_NAMELEN);
			disk_write(&si, sd_cwd.sfd_ino);
			disk_write(&si2, sd2[0].sfd_ino);
			disk_write(sd2, si.sfi_direct[i]);
			disk_write(sd3, si2.sfi_direct[0]);
			return;
		}
		disk_read(sd, si.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if(sd[j].sfd_ino == 0){
				if((sd[j].sfd_ino = findBlock()) == 0){
					error_message("mkdir", org_path, -4);
					return;
				}
				strncpy(sd[j].sfd_name, org_path, SFS_NAMELEN);
				bzero(&si2, SFS_BLOCKSIZE);
				si.sfi_size += 64;
				si2.sfi_size = 128;
				si2.sfi_type = SFS_TYPE_DIR;
				if((si2.sfi_direct[0] = findBlock()) == 0){
					error_message("mkdir", org_path, -4);
					return;
				}
				init_dir(sd3);
				sd3[0].sfd_ino = sd[j].sfd_ino;
				sd3[1].sfd_ino = sd_cwd.sfd_ino;
				strncpy(sd3[0].sfd_name, ".", SFS_NAMELEN);
				strncpy(sd3[1].sfd_name, "..", SFS_NAMELEN);
				disk_write(&si, sd_cwd.sfd_ino);
				disk_write(&si2, sd[j].sfd_ino);
				disk_write(sd, si.sfi_direct[i]);
				disk_write(sd3, si2.sfi_direct[0]);
				return;
			}
			if(!strcmp(sd[j].sfd_name, org_path)){
				error_message("mkdir", org_path, -6);
				return;
			}
		}
	}
	if(i == SFS_NDIRECT){
		error_message("mkdir", org_path, -3);
		return;
	}
}

void sfs_rmdir(const char* org_path) 
{
	int i, j, k;
	struct sfs_inode si, si2;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd2[SFS_DENTRYPERBLOCK];
	if(!strcmp(".", org_path)){
		error_message("rmdir", org_path, -8);
		return;
	}
	if(!strcmp("..", org_path)){
		error_message("rmdir", org_path, -7);
		return;
	}
	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] != 0){
			disk_read(sd,si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino != 0){
					if(!strcmp(sd[j].sfd_name, org_path)){
						disk_read(&si2, sd[j].sfd_ino);
						if(si2.sfi_type == SFS_TYPE_FILE){
							error_message("rmdir", org_path, -5);
							return;
						}
						if(si2.sfi_size == 128){
							bitSet(sd[j].sfd_ino);
							bitSet(si2.sfi_direct[0]);
							sd[j].sfd_ino = SFS_NOINO;
							si2.sfi_direct[0] = SFS_NOINO;
							for(k = 0; k < SFS_NDIRECT; k++){
								if(sd[k].sfd_ino != 0)
									break;
							}
							disk_write(sd, si.sfi_direct[i]);
							if(k == SFS_NDIRECT){
								bitSet(si.sfi_direct[i]);
								si.sfi_direct[i] = SFS_NOINO;
							}
							si.sfi_size -= 64;
							bzero(&si2, SFS_BLOCKSIZE);
							disk_write(&si, sd_cwd.sfd_ino);
							return;
						}
						else{
							error_message("rmdir", org_path, -7);
							return;
						}
					}
				}
			}
		}
	}
	if(i == SFS_NDIRECT){
		error_message("rmdir", org_path, -1);
	}
}

void sfs_mv(const char* src_name, const char* dst_name) 
{
	int i, j;
	struct sfs_inode si;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];
	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino != 0){
					if(!strcmp(sd[j].sfd_name, dst_name)){
						error_message("mv", dst_name, -6); 
						return;
					}
				}
			}
		}
	}
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] != 0){
			init_dir(sd);
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino != 0){
					if(!strcmp(sd[j].sfd_name, src_name)){
						strcpy(sd[j].sfd_name, dst_name);
						disk_write(sd, si.sfi_direct[i]);
						return;
					}
				}
			}
		}
	}
	if(i == SFS_NDIRECT)
			error_message("mv", src_name, -1);
}

void sfs_rm(const char* path) 
{
	int i, j, k;
	struct sfs_inode si, si2;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd2[SFS_DENTRYPERBLOCK];
	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] != 0){
			disk_read(sd,si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino != 0){
					if(!strcmp(sd[j].sfd_name, path)){
						disk_read(&si2, sd[j].sfd_ino);
						if(si2.sfi_type == SFS_TYPE_DIR){
							error_message("rm", path, -9);
							return;
						}
						else{
							bitSet(sd[j].sfd_ino);
							for(k = 0; k < SFS_NDIRECT; k++){
								if(si2.sfi_direct[k] != 0){
									bitSet(si2.sfi_direct[k]);
									si2.sfi_direct[k] = SFS_NOINO;
								}
							}
							if(si2.sfi_indirect != 0){
								bitSet(si2.sfi_indirect);
								si2.sfi_indirect = SFS_NOINO;
							}
							sd[j].sfd_ino = SFS_NOINO;
							disk_write(sd, si.sfi_direct[i]);
							si.sfi_size -= 64;
							bzero(&si2, SFS_BLOCKSIZE);
							disk_write(&si, sd_cwd.sfd_ino);
							return;
						}
					}
				}
			}
		}
	}

	if(i == SFS_NDIRECT){
		error_message("rm", path, -1);
	}
}

void sfs_cpin(const char* local_path, const char* path) 
{
	int i, j, k, l, a, count, readsize;
	count = 0;
	u_int32_t indirect[SFS_DBPERIDB] = {0, };
	int check = 0;
	struct sfs_inode si, si2, si3;
	struct sfs_dir sd[SFS_DENTRYPERBLOCK], sd2[SFS_DENTRYPERBLOCK];
	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] == 0)
			check = 1;
		if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){
					check = 1;
					//error_message("cd", path, -1);
					//return;
				}
				else{
				 if(!strcmp(sd[j].sfd_name, local_path)){
					 error_message("cpin",  local_path, -6);
					 return;
				 }
				}
			}
		}
	}
	if(check == 0){
		error_message("cpin", local_path, -3);
		return;
	}

	fd1 = open(path, O_RDWR);
	if(fd1 < 0){
		printf("cpin: can't open %s input file\n", path);
		return;
	}

	if((lseek(fd1, 0, SEEK_END)) > ((SFS_NDIRECT * SFS_BLOCKSIZE) + (SFS_DBPERIDB * SFS_BLOCKSIZE))){
		printf("cpin: input file size exceeds the max file size\n");
		return;
	}

	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] == 0){
			if( (si.sfi_direct[i] = findBlock()) == 0){
				error_message("cpin", local_path, -4);
				return;
			}
			init_dir(sd);
			if((sd[0].sfd_ino = findBlock()) == 0){
				error_message("cpin", local_path, -4);
				return;
			}
			strncpy(sd[0].sfd_name, local_path, SFS_NAMELEN);
			bzero(&si2, SFS_BLOCKSIZE);
			si.sfi_size += 64;
			si2.sfi_size = 0;
			si2.sfi_type = SFS_TYPE_FILE;
		//	printf("sd_cwd.sfd_ino: %d\n",sd_cwd.sfd_ino);
		//	printf("si.sfi_direct[i]: %d\n",si.sfi_direct[i]);
		//	printf("sd[0].sfd_ino: %d\n",sd[0].sfd_ino);
			disk_write(&si, sd_cwd.sfd_ino);
			disk_write(sd, si.sfi_direct[i]);
			disk_write(&si2, sd[0].sfd_ino);
			for(j = 0; j < SFS_NDIRECT; j++){
				readsize = disk_read1(&si3, count);
				if(readsize == 0)
					return;
				if(si2.sfi_direct[j] == 0){
				if((si2.sfi_direct[j] = findBlock()) == 0){
					error_message("cpin", local_path, -4);
					return;
				}
		//		bzero(&si3, SFS_BLOCKSIZE);
				readsize = disk_read1(&si3, count);
				if(readsize == 0)
					return;
				si2.sfi_size += readsize;
				count++;
		//	printf("sd[0].sfd_ino: %d\n",sd[0].sfd_ino);
		//	printf("si2.sfi_direct[j]: %d\n",si2.sfi_direct[j]);
				disk_write(&si2, sd[0].sfd_ino);
				disk_write(&si3, si2.sfi_direct[j]);
				if(readsize < SFS_BLOCKSIZE){
					return;
				}
				}
			}
			if(j == SFS_NDIRECT){
				if(si2.sfi_indirect == 0){
					if((si2.sfi_indirect = findBlock()) == 0){
						error_message("cpin", local_path, -4);
						return;
					}
					for(k = 0; k < SFS_DBPERIDB; k++){
				//		bzero(&si3, SFS_BLOCKSIZE);
						readsize = disk_read1(&si3, count);
						if(readsize == 0)
							return;
						if(indirect[k] == 0){
						if((indirect[k] = findBlock()) == 0){
							error_message("cpin", local_path, -4);
							return;
						}
						si2.sfi_size += readsize;
						count++;
		//	printf("sd[0].sfd_ino: %d\n",sd[0].sfd_ino);
		//	printf("si2.sfi_indirect: %d\n",si2.sfi_indirect);
		//	printf("indirect[k]: %d\n",indirect[k]);
						disk_write(&si2, sd[0].sfd_ino);
						disk_write(indirect, si2.sfi_indirect);
						disk_write(&si3, indirect[k]);
						if(readsize < SFS_BLOCKSIZE)
							return;
						}
					}
				}
			}
		}
		disk_read(sd, si.sfi_direct[i]);
		for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
			if(sd[j].sfd_ino == 0){
				if((sd[j].sfd_ino = findBlock()) == 0){
					error_message("cpin", local_path, -4);
					return;
				}
				strncpy(sd[j].sfd_name, local_path, SFS_NAMELEN);
				bzero(&si2, SFS_BLOCKSIZE);
				si.sfi_size += 64;
				si2.sfi_size = 0;
				si2.sfi_type = SFS_TYPE_FILE;
	//		printf("sd_cwd.sfd_ino: %d\n",sd_cwd.sfd_ino);
	//		printf("si.sfi_direct[i]: %d\n",si.sfi_direct[i]);
	//		printf("sd[j].sfd_ino: %d\n",sd[j].sfd_ino);
				disk_write(&si, sd_cwd.sfd_ino);
				disk_write(sd, si.sfi_direct[i]);
				disk_write(&si2, sd[j].sfd_ino);
				for(k = 0; k < SFS_NDIRECT; k++){
					readsize = disk_read1(&si3, count);
					if(readsize == 0)
						return;
					if(si2.sfi_direct[k] == 0){
					if((si2.sfi_direct[k] = findBlock()) == 0){
						error_message("cpin", local_path, -4);
						return;
					}
				//	bzero(&si3, SFS_BLOCKSIZE);
					readsize = disk_read1(&si3, count);
					if(readsize == 0)
						return;
					si2.sfi_size += readsize;
					count++;
	//		printf("sd[j].sfd_ino: %d\n",sd[j].sfd_ino);
	//		printf("si2.sfi_direct[k]: %d\n",si2.sfi_direct[k]);
					disk_write(&si2, sd[j].sfd_ino);
					disk_write(&si3, si2.sfi_direct[k]);
					if(readsize < SFS_BLOCKSIZE){
						return;
					}
					}
				}
				if(k == SFS_NDIRECT){
					if(si2.sfi_indirect == 0){
					if((si2.sfi_indirect = findBlock()) == 0){
						error_message("cpin", local_path, -4);
						return;
					}
					for(l = 0; l < SFS_DBPERIDB; l++){
					//	bzero(&si3, SFS_BLOCKSIZE);
						readsize = disk_read1(&si3, count);
						if(readsize == 0)
							return;
						if(indirect[l] == 0){
						if((indirect[l] = findBlock()) == 0){
							error_message("cpin", local_path, -4);
							return;
						}
					//	for(a = 0; a < 128; a++)
						//	printf("%d\t",indirect[a]);
					//	printf("\n");
						si2.sfi_size += readsize;
						count++;
						disk_write(&si2, sd[j].sfd_ino);
						disk_write(indirect, si2.sfi_indirect);
						disk_write(&si3, indirect[l]);
	//		printf("sd[j].sfd_ino: %d\n",sd[j].sfd_ino);
	//		printf("si2.sfi_indirect: %d\n",si2.sfi_indirect);
	//		printf("indirect[%d]: %d\n",l ,indirect[l]);
	//		printf("\n");
						if(readsize < SFS_BLOCKSIZE)
							return;
						}
					}
					}
				}
			}
		}
	}
	if(i == SFS_NDIRECT){
		error_message("cpin", path, -3);
		return;
	}

}

void sfs_cpout(const char* local_path, const char* path) 
{
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];
	struct sfs_inode si, si2, si3;
	int i, j, k;
	int count = 0;
	int size = 0;
	int arr[SFS_DBPERIDB];

	fd1 = open(path, O_RDWR | O_CREAT | O_EXCL);
	if(fd1 < 0){
		error_message("cpout", path, -6);
		return;
	}
	chmod(path, 0644); 

	disk_read(&si, sd_cwd.sfd_ino);
	for(i = 0; i < SFS_NDIRECT; i++){
		if(si.sfi_direct[i] != 0){
			disk_read(sd, si.sfi_direct[i]);
			for(j = 0; j < SFS_DENTRYPERBLOCK; j++){
				if(sd[j].sfd_ino == 0){
					//error_message("cd", path, -1);
					//return;
				}
				else{
				 if(!strcmp(sd[j].sfd_name, local_path) && sd[j].sfd_ino != 0){
					 disk_read(&si2, sd[j].sfd_ino);
					 size = si2.sfi_size;
					 if(si2.sfi_type == 1){
						for(k = 0; k < SFS_NDIRECT; k++){
							if(si2.sfi_direct[k] == 0){
								disk_close1();
								return;
							}
							disk_read(&si3, si2.sfi_direct[k]);
							disk_write1(&si3, count, size);
							count++;
						//	printf("%d\n", size);
							size -= 512;
						}
						if(si2.sfi_indirect == 0){
							disk_close1();
							return;
						}
						disk_read(arr, si2.sfi_indirect);
						for(k = 0; k < SFS_DBPERIDB; k++){
							if(arr[k] == 0){
								disk_close1();
								return;
							}
							disk_read(&si3, arr[k]);
							disk_write1(&si3, count, size);
							count++;
						//	printf("%d\n", size);
							size -= 512;
						}
					 }
					 if(si2.sfi_type == 2){
						 error_message("cpout", local_path, -10);
						return;
					 }
				 }
				}
			}
		}
	}
		error_message("cpout", local_path, -1);
		disk_close1();
		return;
	
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
