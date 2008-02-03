/*
	cbmfs by Michael Steil <mist@c64.org>
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>


#define BLOCKS 683
#define BLOCKSIZE 256
#define MAX_DENTRIES 144
#define NAMEMAX 16
#define BLOCK(n) (d64buf+BLOCKSIZE*n)
#define VOLNAME(j) (BLOCK(linear(18,0))[0x90+j])

#define LINK_TRACK(i) (BLOCK(i)[0])
#define LINK_SECTOR(i) (BLOCK(i)[1])

#define DENTRY(a,b) (BLOCK(a)+b*32)
#define DE_TRACK	0x03
#define DE_SECTOR	0x04
#define DE_NAME		0x05
#define DE_INFO_TRACK 0x15
#define DE_INFO_SECTOR 0x16
#define DE_YEAR		0x19
#define DE_MONTH	0x1A
#define DE_DAY		0x1B
#define DE_HOUR		0x1C
#define DE_MIN		0x1D

unsigned char d64buf[BLOCKSIZE * BLOCKS];

struct dentry_t {
	char name[NAMEMAX+1];
	int size;
	unsigned char *data;
	time_t time;
} dentry[MAX_DENTRIES];

int totalfiles;
char volname[NAMEMAX+1];
int blocksfree;

static int hello_getattr(const char *path, struct stat *stbuf)
{
printf("hello_getattr\n");
    int i;

    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
printf("hello_getattr1\n");
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
		return 0;
    } else {	
		for (i=0; i<totalfiles; i++) {
printf("hello_getattr: %s\n", path);
			if(strcmp(path+1, dentry[i].name) == 0) {
				stbuf->st_mode = S_IFREG | 0444;
				stbuf->st_nlink = 1;
				stbuf->st_size = dentry[i].size;
				stbuf->st_mtime = dentry[i].time;
				return 0;
			}
		}
	}
    return -ENOENT;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi)
{
printf("hello_readdir\n");
    (void) offset;
    (void) fi;

	int i;

    if(strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
//    filler(buf, hello_path + 1, NULL, 0);

	for (i=0; i<totalfiles; i++) {
		filler(buf, dentry[i].name, NULL, 0);
	}

    return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
printf("hello_open\n");
	int i;

	for (i=0; i<totalfiles; i++) {
		if(strcmp(path+1, dentry[i].name) == 0) {
			if((fi->flags & 3) != O_RDONLY)
				return -EACCES;

			return 0;
		}
	}
    return -ENOENT;
}

static int hello_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi)
{
printf("hello_read\n");
    size_t len;
    (void) fi;
	int i;

	for (i=0; i<totalfiles; i++) {
		if(strcmp(path+1, dentry[i].name) == 0) {
			len = dentry[i].size;
			if (offset < len) {
				if (offset + size > len)
					size = len - offset;
				memcpy(buf, dentry[i].data + offset, size);
			} else
				size = 0;

			return size;
		}
	}

    return -ENOENT;
}

static void *
hello_init(struct fuse_conn_info *conn)
{
	printf("hello_init\n");
    return NULL;
}

static int
hello_opendir(const char *path, struct fuse_file_info *fi)
{
	printf("hello_opendir\n");
    return 0;
}


static int
hello_statfs(const char *path, struct statvfs *buf)
{
	printf("hello_statfs\n");
    (void)path;

    buf->f_namemax = NAMEMAX;
    buf->f_bsize = 256;
    buf->f_frsize = 256; // ??
    buf->f_blocks = 664/2; // 256 vs. 512
	buf->f_bfree = buf->f_bavail = blocksfree/2; //256 vs. 512
    buf->f_files = totalfiles;
	buf->f_ffree = 0;
    return 0;
}

#if 0
static int
hello_listxattr(const char *path, char *list, size_t size)
{
    if (resource_fork_content == 0)
        return -ENOENT;

    if (strcmp(path, file_path) != 0)
        return -ENOENT;

    if (size > 0)
    {
        memcpy(list, finder_info_xattr, sizeof(finder_info_xattr));
        memcpy(list + sizeof(finder_info_xattr),
            resource_fork_xattr, sizeof(resource_fork_xattr));
    }

    return sizeof(finder_info_xattr) + sizeof(resource_fork_xattr);
}

static int
hello_getxattr(const char *path, const char *name, char *value, size_t size)
{
    if (resource_fork_content == 0)
        return -ENOENT;

    if (strcmp(path, file_path) != 0)
        return -ENOENT;

    if (strcmp(name, finder_info_xattr) == 0)
    {
        if (value != 0 && size >= finder_info_size)
            memcpy(value, custom_icon_finder_info, finder_info_size);
        return finder_info_size;
    }
    else if (strcmp(name, resource_fork_xattr) == 0)
    {
        if (value != 0 && size >= resource_fork_size)
            memcpy(value, resource_fork_content, resource_fork_size);
        return resource_fork_size;
    }
    else return 0;
}
#endif

static struct fuse_operations hello_oper = {
	.init	= hello_init,
	.statfs	= hello_statfs,
	.opendir	= hello_opendir,
    .getattr	= hello_getattr,
    .readdir	= hello_readdir,
    .open	= hello_open,
    .read	= hello_read,
#if 0
    .listxattr = hello_listxattr,
    .getxattr  = hello_getxattr
#endif
};

int linear(int track, int sector) {
	track--;
	if (track<17)
			return sector + track*21;
	if (track<24)
			return sector + (track-17)*19 + 17*21;
	if (track<30)
			return sector + (track-24)*18 + (24-17)*19 + 17*21;
	return sector + (track-30)*17 + (30-24)*18 + (24-17)*19 + 17*21;
}

unsigned char *follow(int l, int *len) {
	*len = 0;
	unsigned char *file = malloc(BLOCKS*BLOCKSIZE); // worst case
	unsigned char *data = NULL;
	char fn[8];
	unsigned char *p = file;

	do {
		if (linear(LINK_TRACK(l),LINK_SECTOR(l))==l)
			break; /* loop!*/
		if (LINK_TRACK(l) == 0) {
			memcpy(p, BLOCK(l)+2, LINK_SECTOR(l)-1);
			p+=LINK_SECTOR(l)-1;
			break;
		} else if (LINK_TRACK(l)>35) {
			memcpy(p, BLOCK(l), BLOCKSIZE);
			printf("ILLEGAL TRACK %i\n", LINK_TRACK(l));
			goto out;
		} else { /* we should check here whether the sector number is legal */
			memcpy(p, BLOCK(l)+2, BLOCKSIZE-2);
			p+=BLOCKSIZE-2;
			l = linear(LINK_TRACK(l), LINK_SECTOR(l));
		}
	} while(1);
	*len = p - file;
	data = malloc(*len);
	memcpy(data, file, *len);
out:
	free(file);
	return data;
}

static char *def_opts = "-oallow_other,ro,local,noappledouble,volname=%s";

int main(int argc, char *argv[])
{
    int i;
    char **new_argv;

	FILE *f = fopen(argv[1], "r");
	if (!f) {
		printf("error opening: %s\n", argv[1]);
		return 1;
	}
	fread(d64buf, 1, sizeof(d64buf), f);
	fclose(f);

	int j;
	int dir;

	for (j=0; j<NAMEMAX; j++) {
		if (VOLNAME(j)==0xA0) break;
		volname[j] = VOLNAME(j);
	}
	volname[j]=0;
	printf("volname: %s\n", volname);

	dir = linear(18,1);
	int filenum = 0;
	while(1) {
		for (i=0; i<8; i++) { /* dir entries */
			if (DENTRY(dir, i)[2]) { /* not deleted */
				for (j=0; j<NAMEMAX; j++) {
					if (DENTRY(dir, i)[j+DE_NAME]==0xA0) break;
					dentry[filenum].name[j] = DENTRY(dir, i)[j+DE_NAME];
				}
				dentry[filenum].name[j] = 0;
				printf("%s\n", dentry[filenum].name);

				dentry[filenum].data = follow(linear(DENTRY(dir, i)[DE_TRACK], DENTRY(dir, i)[DE_SECTOR]), &dentry[filenum].size);

				struct tm timeptr = {
					.tm_sec = 0,
					.tm_min = DENTRY(dir, i)[DE_MIN],
					.tm_hour = DENTRY(dir, i)[DE_HOUR],
					.tm_mday = DENTRY(dir, i)[DE_DAY],
					.tm_mon = DENTRY(dir, i)[DE_MONTH],
					.tm_year = DENTRY(dir, i)[DE_YEAR]
				};
				dentry[filenum].time = timegm(&timeptr);

//				printf("%d, %d\n", );

				for (j=0xA0; j<=0xFF; j++) {
					char c = BLOCK(linear(DENTRY(dir, i)[DE_INFO_TRACK], DENTRY(dir, i)[DE_INFO_SECTOR]))[j];
					if (!c) break;
					printf("%c", c);
				}
				printf("\n");

				filenum++;
			}
		}
		if (!LINK_TRACK(dir)) break;
		dir = linear(LINK_TRACK(dir),LINK_SECTOR(dir));
	}

	totalfiles = filenum;

	blocksfree = 0;
	for (i=1; i<=35; i++) {
		if (i!=18)
			blocksfree += BLOCK(linear(18,0))[4*i];
	}

	char opts[100];
	snprintf(opts, sizeof(opts), def_opts, volname);
	argv[1] = opts;
    return fuse_main(argc, argv, &hello_oper, NULL);
}
