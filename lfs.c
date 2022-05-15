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

static struct entry *entries[MAX_ENTRIES]; //Array of files and directories

struct entry *getEntry(const char *path);
int getEmptyIndex();
char *getPath(const char *full_path);
char *getName(const char *full_path);
int getEntryIndex(const char *path);

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
		if(entries[i])
		  if (strcmp(entries[i]->full_path, path) == 0) {
			printf("Found entry: %s\n",path);
			return entries[i];
		  }
	}
	return res;
}

int getEntryIndex(const char *path) {
	for (int i = 0; i < MAX_ENTRIES; i++) {
		if(entries[i])
		  if (strcmp(entries[i]->full_path, path) == 0) {
			printf("Found entry index: %s\n",path);
			return i;
		  }
	}
	return -1;
}

char *getPath(const char *full_path) {
	char *name = getName(full_path);
	size_t name_length = strlen(name);
	size_t full_length = strlen(full_path);
	free(name);

	size_t i = 1;

	if (full_length - name_length == 1)
	  i = 0;

	char *path = calloc(sizeof(char),full_length - name_length - i);
	if(!path) {
		return NULL;
	}
	memcpy(path, full_path, full_length - name_length - i);

	return path;
}

char *getName(const char *full_path) {
	size_t length = 0;
	size_t full_length = strlen(full_path);
	for(size_t i = 0; i < full_length; i++) {
		if(full_path[i] == '/') 
		  length = i;
	}

	char *name = calloc(sizeof(char), full_length - length);
	if(!name) {
		return NULL;
	}
	memcpy(name, full_path + length+1, full_length - length);

	return name;
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
			printf("entry not found\n");
			return -ENOENT;
		}

		printf("entry found: %s\n",ent->name);

		if(ent->isDir == 1){
			stbuf->st_mode = S_IFDIR | 0755;
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
	printf("readdir: (path=%s)\n", path);

	if(strcmp(path, "/") != 0) {
		int dir_exists = 0;
		for(int i = 0; i < MAX_ENTRIES; i++) {
			if(entries[i])
				if(strcmp(entries[i]->full_path, path) == 0)
					dir_exists = 1;
		}
		
		if(!dir_exists)
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	for(int i = 0; i < MAX_ENTRIES; i++) {
		if(entries[i])
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
	printf("mknod: (path=%s)\n", path);
	int i = getEmptyIndex();
	if(i < 0){
		return -ENOSPC; // Does this error message make sence?
	}
	entries[i] = calloc(sizeof(struct entry), 1);
	if(!entries[i]){
		return -ENOMEM;
	}

	struct entry *ent = entries[i];
	ent->size = 0;
	ent->full_path = calloc(sizeof(char),strlen(path));
	if(!ent->full_path){
		return -ENOMEM;
	}
	strcpy(ent->full_path, path);
	printf("full path = %s\n", ent->full_path);
	ent->isDir = 0;
	ent->atime = time (NULL);
	ent->mtime = time (NULL);
	ent->ctime = time (NULL);
	ent->name = getName(path);
	printf("name = %s\n", ent->name);
	ent->path = getPath(path);
	printf("path = %s\n", ent->path);
	ent->content = NULL;
	

	return 0;
}

/** Create a directory 
 *
 * Note that the mode argument may not have the type specification
 * bits set, i.e. S_ISDIR(mode) can be false.  To obtain the
 * correct directory type bits use  mode|S_IFDIR
 * */
int lfs_mkdir(const char *path, mode_t mode) {
	printf("making dir = %s\n", path);
	int i = getEmptyIndex();
	if(i < 0){
		return -ENOSPC; // Does this error message make sence?
	}
	entries[i] = calloc(sizeof(struct entry), 1);
	if(!entries[i]){
		return -ENOMEM;
	}

	struct entry *ent = entries[i];

	ent->size = 0;
	ent->full_path = calloc(sizeof(char),strlen(path));
	strcpy(ent->full_path, path);
	printf("full path = %s\n", ent->full_path);
	ent->isDir = 1;
	ent->atime = time (NULL);
	ent->mtime = time (NULL);
	ent->ctime = time (NULL);
	ent->name = getName(path);
	printf("name = %s\n", ent->name);
	ent->path = getPath(path);
	printf("path = %s\n", ent->path);
	ent->content = NULL;

	return 0;
}

/** Remove a file */
int lfs_unlink(const char *path) {
	int i = getEntryIndex(path);
	if(i < 0)
		return 0;

	if(entries[i]->content)
		free(entries[i]->content);
	if(entries[i]->name)
		free(entries[i]->name);
	if(entries[i]->path)
		free(entries[i]->path);
	if(entries[i]->full_path)
		free(entries[i]->full_path);
	
	free(entries[i]);
	entries[i] = NULL;

	return 0;
}

/** Remove a directory */
int lfs_rmdir(const char *path) {
	int i = getEntryIndex(path);
	if(i < 0)
		return 0;

	if(entries[i]->content)
		free(entries[i]->content);
	if(entries[i]->name)
		free(entries[i]->name);
	if(entries[i]->path)
		free(entries[i]->path);
	if(entries[i]->full_path)
		free(entries[i]->full_path);
	
	free(entries[i]);
	entries[i] = NULL;

	return 0;
}

//Permission
int lfs_open( const char *path, struct fuse_file_info *fi ) {
    printf("open: (path=%s)\n", path);
	
	if(strcmp(path, "/")) {
		fi->fh = (uint64_t) entries[0];
	} else {
		struct entry *ent = getEntry(path);
		if(!ent){
			//fi->fh = (uint64_t) ent;
			return -ENOENT;
		}

	}

	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	struct entry *ent = getEntry(path);
	//struct entry *ent = (struct entry *) fi->fh;
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
	struct entry *ent = getEntry(path);
	//struct entry *ent = (struct entry *) fi->fh;
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

	if(argc < 3) {
		return -1;
	}

	FILE *file;
	int isDir[1];
	size_t size[1];
	time_t time[1];
	int count[1];

	size_t l;

	file = fopen(argv[3], "rb");
	if(!file){
		return -1;
	}
	// Read the file into memory 
	
	int i = 0;
	l = fread(count, sizeof(int), 1, file);
	if(l > 0 && count[0] > 0 && count[0] <= MAX_ENTRIES) {
		while(i < count[0]) {
			entries[i] = calloc(sizeof(struct entry), 1);
			if(!entries[i]){
				return -ENOMEM;
			}
			l = fread(size, sizeof(size_t), 1, file);
			entries[i]->full_path = calloc(sizeof(char), size[0]);
			if(!entries[i]->full_path) {
				return -ENOMEM;
			}
			l = fread(entries[i]->full_path, sizeof(char), size[0], file);
			printf("found: %s\n", entries[i]->full_path);
			entries[i]->name = getName(entries[i]->full_path);
			entries[i]->path = getPath(entries[i]->full_path);

			l = fread(isDir, sizeof(int), 1, file);
			entries[i]->isDir = isDir[0];

			l = fread(time, sizeof(time_t), 1, file);
			entries[i]->atime = time[0];
			
			l = fread(time, sizeof(time_t), 1, file);
			entries[i]->mtime = time[0];
			
			l = fread(time, sizeof(time_t), 1, file);
			entries[i]->ctime = time[0];
			
			l = fread(size, sizeof(size_t), 1, file);
			entries[i]->size = (off_t) size[0];

			if(!entries[i]->isDir && entries[i]->size > 0) {
				entries[i]->content = calloc(sizeof(char), entries[i]->size);
				l = fread(entries[i]->content, sizeof(char), entries[i]->size, file);
			}

			i++;

		}
	}
	

	fclose(file);
	


	fuse_main( 3, argv, &lfs_oper);


	file = fopen(argv[3], "wb");
	if(!file){
		return -1;
	}

	
	count[0] = 0;

	for(int j = 0; j < MAX_ENTRIES; j++) {
		if(entries[j]) {
			count[0]++;
		}
	}

	fwrite(count, sizeof(int), 1, file);

	for(int j = 0; j < MAX_ENTRIES; j++) {
		if(entries[j]){

			//fwrite(entries[j]->size, sizeof(size_t), 1, file);
			//		entries[i] = calloc(sizeof(struct entry), 1);

			size[0] = strlen(entries[j]->full_path);
			fwrite(size, sizeof(size_t), 1, file);
			fwrite(entries[j]->full_path, sizeof(char), size[0], file);
			
			isDir[0] = entries[j]->isDir;
			fwrite(isDir, sizeof(int), 1, file);

			time[0] = entries[j]->atime;
			fwrite(time, sizeof(time_t), 1, file);

			time[0] = entries[j]->mtime;
			fwrite(time, sizeof(time_t), 1, file);

			time[0] = entries[j]->ctime;
			fwrite(time, sizeof(time_t), 1, file);

			size[0] = (size_t) entries[j]->size;
			fwrite(size, sizeof(size_t), 1, file);

			if(!entries[j]->isDir && entries[j]->size > 0) {
				fwrite(entries[j]->content, sizeof(char), entries[j]->size, file);
			}

			free(entries[j]->content);
			free(entries[j]->name);
			free(entries[j]->path);
			free(entries[j]->full_path);
			free(entries[j]);
		}
	}

	fclose(file);

	return 0;
}
