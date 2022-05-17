#include <fuse.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ENTRIES 1000 //Maximum amount of files and directories allowed

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
int lfs_rename(const char *from, const char *to);
int lfs_truncate(const char *path, off_t size);
int lfs_utime(const char *path, struct utimbuf *buf);

struct entry {
	char *full_path;//The full path of the entry
	char *path; 	//The path up to the parent directory
	char *name; 	//Name of the entry without the path
	int isDir;		//Pracitacly boolean
	time_t mtime, atime, ctime; //Last modified, accessed, and created times
	off_t size;		//Length of the content
	char *content;	//The content of the file
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
	.truncate = lfs_truncate,
	.open	= lfs_open,
	.read	= lfs_read,
	.release = lfs_release,
	.write = lfs_write,
	.rename = lfs_rename,
	.utime = lfs_utime
};

// Returns the index of the first empty entry in the entries array
int getEmptyIndex() {
	for(int i = 0; i < MAX_ENTRIES; i++){
		if(!entries[i]){
			return i;
		}
	}
	return -1;
}

// Takes a path and returns the coresponding entry, if any
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
// Takes a full path and returns an index of the entry in the entries array
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
// Returns the path from the full path
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

//Returs the name from a full path
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

// 
int lfs_getattr( const char *path, struct stat *stbuf ) {
	int res = 0;
	printf("getattr: (path=%s)\n", path);
	
	memset(stbuf, 0, sizeof(struct stat)); //Initialize the stat struct
	if( strcmp( path, "/" ) == 0 ) {	   //If the path is the root directory
		stbuf->st_mode = S_IFDIR | 0755;   //Set the mode to be a directory
		stbuf->st_nlink = 2;			   //Set the number of links to be 2
	} else {
		struct entry *ent = getEntry(path);//Get the entry

		if(!ent){
			printf("entry not found\n");   
			return -ENOENT;
		}

		printf("entry found: %s\n",ent->name); //Print the entry name

		if(ent->isDir == 1){
			stbuf->st_mode = S_IFDIR | 0755;  //Set the mode to be a directory
			stbuf->st_nlink = 2;
		} else {
			stbuf->st_mode = S_IFREG | 0777; //Set the mode to be a file
			stbuf->st_nlink = 1;
		}
		stbuf->st_size = ent->size;  //Set the size of the file
		stbuf->st_mtime = ent->mtime;//Set the modified time
		stbuf->st_atime = ent->atime;//Set the accessed time
		stbuf->st_ctime = ent->ctime;//Set the created time
	} 

	return res;
}

int lfs_readdir( const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi ) {
	printf("readdir: (path=%s)\n", path);


	//Check if the directory exists 
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
	for(int i = 0; i < MAX_ENTRIES; i++) { //Fills in all the entries in that directory
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
	if(i < 0){ //If there is no empty entry
		return -ENOSPC;
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
	ent->atime = time (NULL); //Set the accessed time
	ent->mtime = time (NULL); //Set the modified time
	ent->ctime = time (NULL); //Set the created time
	ent->name = getName(path);//Get the name of the file
	if(!ent->name){
		return -ENOMEM;
	}
	printf("name = %s\n", ent->name);
	ent->path = getPath(path);
	if(!ent->path){
		return -ENOMEM;
	}
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
		return -ENOSPC;
	}
	entries[i] = calloc(sizeof(struct entry), 1);
	if(!entries[i]){
		return -ENOMEM;
	}

	struct entry *ent = entries[i];

	ent->size = 0;
	ent->full_path = calloc(sizeof(char),strlen(path)); //Allocate memory for the full path
	strcpy(ent->full_path, path);
	printf("full path = %s\n", ent->full_path); //
	ent->isDir = 1;
	ent->atime = time (NULL); //Set the accessed time
	ent->mtime = time (NULL); //Set the modified time
	ent->ctime = time (NULL); //Set the created time
	ent->name = getName(path);//Get the name of the file
	if(!ent->name){
		return -ENOMEM;
	}
	printf("name = %s\n", ent->name);
	ent->path = getPath(path);
	if(!ent->path){
		return -ENOMEM;
	}
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
	
	if(strcmp(path, "/") != 0) {
		struct entry *ent = getEntry(path);
		if(ent){
			fi->fh = (uint64_t) ent; //Gives the file handler to the caller
		}
	}

	return 0;
}

int lfs_read( const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi ) {
    printf("read: (path=%s)\n", path);
	struct entry *ent = (struct entry *) fi->fh; //Gets the file from the file handle
	if(!ent){
		return -ENOENT;
	}

	if(size > ent->size) { //Trauncates the read size if it is too large
		size = ent->size;
	}

	memcpy( buf, ent->content, size ); //Copies the content to the buffer
	ent->atime = time(NULL); //Updates the access time
	return size;
}

//Release does nothing since no action is needed to release
int lfs_release(const char *path, struct fuse_file_info *fi) {
	printf("release: (path=%s)\n", path);
	return 0;
}

int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
	printf("write: (path=%s)\n", path);
	struct entry *ent = (struct entry *) fi->fh; //Gets the file from the file handeler 
	
	if(!ent){
		return -ENOENT;
	}

	if(ent->content){
		free(ent->content); //Free the old content
	}

	ent->content = calloc(sizeof(char), size); //Allocate memory for the new content
	if(!ent->content){
		return -ENOMEM;
	}

	memcpy(ent->content, buf, size); //Copies the content to the new content
	ent->mtime = time(NULL); //Updates the modified time
	ent->atime = time(NULL); //Updates the access time
	ent->size = size;		 //Updates the size

	return size;
}

int lfs_rename(const char* from, const char* to) {
	printf("rename: path: %s to: %s\n", from, to);
	struct entry *ent = getEntry(from); //Gets the entry
	if(!ent){
		return -ENOENT;
	}

	if(strcmp(from, to) != 0) { //If the two names are not the same
		int old = getEntryIndex(to); //Checks for an entry with the same name

		free(ent->full_path);
		free(ent->name);
		free(ent->path);

		ent->full_path = calloc(sizeof(char),strlen(to));
		if(!ent->full_path){
			return -ENOMEM;
		}
		strcpy(ent->full_path, to); //Sets the new path
		printf("full path = %s\n", ent->full_path);
		ent->name = getName(to); //Setes the new name
		if(!ent->name){
			return -ENOMEM;
		}
		printf("name = %s\n", ent->name);
		ent->path = getPath(to); //Setes the new path to parent dir
		if(!ent->path){
			return -ENOMEM;
		}
		printf("path = %s\n", ent->path);
		ent->mtime = time(NULL); //Updates modification time 
		ent->atime = time(NULL); //Updates access time

		if(entries[old]){ //If an existing entry matches the new name, remove that entry
			if(entries[old]->content)
				free(entries[old]->content);
			if(entries[old]->name)
				free(entries[old]->name);
			if(entries[old]->path)
				free(entries[old]->path);
			if(entries[old]->full_path)
				free(entries[old]->full_path);
			
			free(entries[old]);
			entries[old] = NULL;
		}
	}

	return 0;
}

int lfs_truncate(const char* path, off_t size) {
	printf("trauncate: (path=%s)\n", path);
	struct entry *ent = getEntry(path);
	if(!ent){
		return -ENOENT;
	}

	char *new_content = calloc(sizeof(char), size); //Allocates new buffer
	if(!new_content){
		return -ENOMEM;
	}

	if(ent->content) {
		if(ent->size > size) {  //Writes any old content into the new buffer 
			memcpy(new_content, ent->content, size);
		} else {
			memcpy(new_content, ent->content, ent->size);
		}

		free(ent->content); //Dealocates the old content buffer 
	}

	ent->content = new_content;

	ent->size = size; 		 //Updates the size

	ent->mtime = time(NULL); //Updates the modified time
	ent->atime = time(NULL); //Updates the access time

	return 0;
}

int lfs_utime(const char *path, struct utimbuf *buf) {
	printf("utime: (path=%s)\n", path);
	struct entry *ent = getEntry(path); //Gets entry
	if(!ent){
		return -ENOENT;
	}

	if(buf) {
		ent->atime = buf->actime; //Updates the modified time
		ent->mtime = buf->modtime; //Updates the access time
	}

	return 0;
}

int main( int argc, char *argv[] ) {

	if(argc < 3) {
		return -1;
	}

	FILE *file;
	size_t size;
	int count; 

	size_t l;

	file = fopen(argv[3], "rb"); //Opens the file for reading
	if(!file){
		return -1;
	}
	// Read the file into memory 
	
	int i = 0;
	l = fread(&count, sizeof(int), 1, file); //Reads the number of entries
	if(l > 0 && count > 0 && count <= MAX_ENTRIES) {
		while(i < count) {
			entries[i] = calloc(sizeof(struct entry), 1);
			if(!entries[i]){
				return -ENOMEM;
			}
			l = fread(&size, sizeof(size_t), 1, file); //Reads the path length for the entry
			entries[i]->full_path = calloc(sizeof(char), size);
			if(!entries[i]->full_path) {
				return -ENOMEM;
			}
			l = fread(entries[i]->full_path, sizeof(char), size, file); //Reads the path
			printf("found: %s\n", entries[i]->full_path);
			entries[i]->name = getName(entries[i]->full_path); //Gets the name of the entry
			entries[i]->path = getPath(entries[i]->full_path); //Gets the path of the entry

			l = fread(&entries[i]->isDir, sizeof(int), 1, file); 	//Reads the isDir value
			l = fread(&entries[i]->atime, sizeof(time_t), 1, file); //Reads the access time
			l = fread(&entries[i]->mtime, sizeof(time_t), 1, file); //Reads the modified time
			l = fread(&entries[i]->ctime, sizeof(time_t), 1, file); //Reads the creation time
			l = fread(&entries[i]->size, sizeof(size_t), 1, file);  //Reads the size

			if(!entries[i]->isDir && entries[i]->size > 0) {
				entries[i]->content = calloc(sizeof(char), entries[i]->size); //Allocates the content
				l = fread(entries[i]->content, sizeof(char), entries[i]->size, file); //Reads the content
			}

			i++; //Increments the index of the entry array 

		}
	}
	

	fclose(file);


	fuse_main( 3, argv, &lfs_oper);


	file = fopen(argv[3], "wb"); //Opens the file for writing
	if(!file){
		return -1;
	}

	
	count = 0;

	for(int j = 0; j < MAX_ENTRIES; j++) {
		if(entries[j]) {
			count++;
		}
	}

	fwrite(&count, sizeof(int), 1, file); //Writes the number of entries

	for(int j = 0; j < MAX_ENTRIES; j++) {
		if(entries[j]){
			size = strlen(entries[j]->full_path);
			fwrite(&size, sizeof(size_t), 1, file); 				 // Writes the length of the full path
			fwrite(entries[j]->full_path, sizeof(char), size, file); // Writes the full path
			fwrite(&entries[j]->isDir, sizeof(int), 1, file);		 // Writes the isDir value
			fwrite(&entries[j]->atime, sizeof(time_t), 1, file);	 // Writes the access time
			fwrite(&entries[j]->mtime, sizeof(time_t), 1, file);	 // Writes the modified time
			fwrite(&entries[j]->ctime, sizeof(time_t), 1, file);	 // Writes the creation time
			fwrite(&entries[j]->size, sizeof(size_t), 1, file);	     // Writes the size

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

	return 0; //Success
}