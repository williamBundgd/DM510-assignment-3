#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ENTRIES 1000

int lfs_getattr( const char *, struct stat * );
int lfs_readdir( const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info * );
int lfs_open( const char *, struct fuse_file_info * );
int lfs_read( const char *, char *, size_t, off_t, struct fuse_file_info * );
int lfs_release(const char *path, struct fuse_file_info *fi);
int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int lfs_mknod(const char *path, mode_t mode, dev_t rdev);
int lfs_mkdir(const char *path, mode_t mode);
int lfs_unlink(const char *path);
int lfs_rmdir(const char *path);

struct entry {
	char *full_path;
	char *path;
	char *name;
	int isDir;
	time_t mtime, atime, ctime;
	off_t size;
	char *content;
};

struct entry **entries; //Array of files and directories

struct entry *getEntry(const char *path);
int getEmptyIndex();

static struct fuse_operations lfs_oper = {
	.getattr	= lfs_getattr,
	.readdir	= lfs_readdir,
	.mknod = lfs_mknod,
	.mkdir = lfs_mkdir,
	.unlink = lfs_unlink,
	.rmdir = lfs_rmdir,
	.truncate = NULL,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = lfs_write,
	.rename = NULL,
	.utime = NULL
};

int getEmptyIndex() {
	for(int i = 0; i < MAX_ENTRIES; i++){
		if(!entries[i]){
			return i;
		}
	}
	return -1;
}

struct entry *getEntry(const char *path) {
	struct entry *res = NULL;

	for (int i = 0; i < MAX_ENTRIES; i++) {
		if (strcmp(entries[i]->full_path, path) == 0)
		  res = entries[i];
	}
	return res;
}

int lfs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path);

	memset(stbuf, 0, sizeof(struct stat));
	if( strcmp( path, "/" ) == 0 ) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		struct entry *ent = getEntry(path);

		if(!ent){
			return -ENOENT;
		}

		if(ent->isDir){
			stbuf->st_mode = S_IFREG | 0755;
			stbuf->st_nlink = 2;
		} else {
			stbuf->st_mode = S_IFREG | 0777;
			stbuf->st_nlink = 1;
		}
		stbuf->st_size = ent->size;
		stbuf->st_mtime = ent->mtime;
		stbuf->st_atime = ent->atime;
		stbuf->st_ctime = ent->ctime;
	} 

	return res;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	(void) offset;
	(void) fi;
	printf("readdir: (path=%s)\n", path);

	int dir_exists = 0;
	for(int i = 0; i < MAX_ENTRIES; i++) {
		if(strcmp(entries[i]->full_path, path) == 0)
		  dir_exists = 1;
	}
	
	if(!dir_exists)
	 return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for(int i = 0; i < MAX_ENTRIES; i++) {
		if(strcmp(entries[i]->path, path) == 0) {
			filler(buf, entries[i]->name, NULL, 0);
		}
	}

	return 0;
}

/** Create a file node
 *
 * This is called for creation of all non-directory, non-symlink
 * nodes.  If the filesystem defines a create() method, then for
 * regular files that will be called instead.
 */
int lfs_mknod(const char *path, mode_t mode, dev_t rdev) {
	(void)mode;
	(void)rdev;
	printf("mknod: (path=%s)\n", path);
	int i = getEmptyIndex();
	if(i < 0){
		return -ENOMEM; // Does this error message make sence?
	}
	entries[i] = calloc(sizeof(struct entry), 1);
	if(!entries[i]){
		return -ENOMEM;
	}

	struct entry *ent = entries[i];

	ent->size = 0;
	ent->full_path = calloc(sizeof(char),strlen(path));
	strcpy(ent->full_path, path);
	ent->isDir = 0;
	ent->atime = time (NULL);
	ent->mtime = time (NULL);
	ent->ctime = time (NULL);
	ent->name = strrchr(path,'/'); // Is this right ????
	size_t pathLen = strlen(path)-strlen(ent->name);
	ent->path = calloc(sizeof(char),pathLen);
	memcpy(ent->path, path, pathLen);

	return 0;
}

/** Create a directory 
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 * */
int lfs_mkdir(const char *path, mode_t mode) {
	(void)mode;
	int i = getEmptyIndex();
	if(i < 0){
		return -ENOMEM; // Does this error message make sence?
	}
	entries[i] = calloc(sizeof(struct entry), 1);
	if(!entries[i]){
		return -ENOMEM;
	}

	struct entry *ent = entries[i];

	ent->size = 0;
	ent->full_path = calloc(sizeof(char),strlen(path));
	strcpy(ent->full_path, path);
	ent->isDir = 1;
	ent->atime = time (NULL);
	ent->mtime = time (NULL);
	ent->ctime = time (NULL);
	ent->name = strrchr(path,'/'); // Is this right ????
	size_t pathLen = strlen(path)-strlen(ent->name);
	ent->path = calloc(sizeof(char),pathLen);
	memcpy(ent->path, path, pathLen);
	return 0;
}

/** Remove a file */
int lfs_unlink(const char *path) {
		struct entry *ent = getEntry(path);
	if(!ent)
		return 0;

	if(ent->content)
		free(ent->content);
	if(ent->name)
		free(ent->name);
	if(ent->path)
		free(ent->path);
	if(ent->full_path)
		free(ent->full_path);
	
	free(ent);

	return 0;
}

/** Remove a directory */
int lfs_rmdir(const char *path) {
	struct entry *ent = getEntry(path);
	if(!ent)
		return 0;

	if(ent->content)
		free(ent->content);
	if(ent->name)
		free(ent->name);
	if(ent->path)
		free(ent->path);
	if(ent->full_path)
		free(ent->full_path);
	
	free(ent);

	return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	
	struct entry *ent = getEntry(path);
	if(ent){
		fi->fh = (uint64_t) ent;
	}
	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	//struct entry *ent = getEntry(path);
	struct entry *ent = (struct entry *) fi->fh;
	if(!ent){
		return -ENOENT;
	}

	if(size > ent->size) {
		size = ent->size;
	}

	memcpy( buf, ent->content, size );
	ent->atime = time(NULL);
	return size;
}

int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("write: (path=%s)\n", path);
	//struct entry *ent = getEntry(path);
	struct entry *ent = (struct entry *) fi->fh;
	if(!ent){
		return -ENOENT;
	}

	if(ent->content){
		free(ent->content);
	}

	ent->content = calloc(sizeof(char), size);
	if(!ent->content){
		return -ENOMEM;
	}

	memcpy(ent->content, buf, size);
	ent->mtime = time(NULL);
	ent->atime = time(NULL);
	ent->size = size;

	return size;
}

int main( int argc, char *argv[] ) {

	entries = calloc(sizeof(struct entry),MAX_ENTRIES);
	if(!entries)
		return -ENOMEM;

	entries[0] = calloc(sizeof(struct entry), 1);
	if(!entries[0]){
		free(entries);
		return -ENOMEM;
	}

	entries[0]->full_path = "/";
	entries[0]->path = "/";
	entries[0]->name = "/";

	fuse_main( argc, argv, &lfs_oper );

	for(int i = 0; i < MAX_ENTRIES; i++){
		if(entries[i]) {
			if(entries[i]->content)
				free(entries[i]->content);
			if(entries[i]->name)
				free(entries[i]->name);
			if(entries[i]->path)
				free(entries[i]->path);
			if(entries[i]->full_path)
				free(entries[i]->full_path);
			free(entries[i]);
		}
	}
	free(entries);

	return 0;
}
