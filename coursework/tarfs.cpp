/*
 * TAR File-system Driver
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (4)
 */

/*
 * STUDENT NUMBER: s1870697
 * 
 */
#include "tarfs.h"
#include <infos/kernel/log.h>


using namespace infos::fs;
using namespace infos::drivers;
using namespace infos::drivers::block;
using namespace infos::kernel;
using namespace infos::util;
using namespace tarfs;

#define DIRECTORY_FLAG '5'


/**
 * TAR files contain header data encoded as octal values in ASCII.  This function
 * converts this terrible representation into a real unsigned integer.
 *
 * You DO NOT need to modify this function.
 *
 * @param data The (null-terminated) ASCII data containing an octal number.
 * @return Returns an unsigned integer number, corresponding to the input data.
 */
static inline unsigned int octal2ui(const char *data)
{
	// Current working value.
	unsigned int value = 0;

	// Length of the input data.
	int len = strlen(data);

	// Starting at i = 1, with a factor of one.
	int i = 1, factor = 1;
	while (i < len) {
		// Extract the current character we're working on (backwards from the end).
		char ch = data[len - i];

		// Add the value of the character, multipled by the factor, to
		// the working value.
		value += factor * (ch - '0');
		
		// Increment the factor by multiplying it by eight.
		factor *= 8;
		
		// Increment the current character position.
		i++;
	}

	// Return the current working value.
	return value;
}

// The structure that represents the header block present in
// TAR files.  A header block occurs before every file, this
// this structure must EXACTLY match the layout as described
// in the TAR file format description.
namespace tarfs {
	struct posix_header {
		                  /* byte offset - Description */
  char name[100];               /* 0   - File name */
  char mode[8];                 /* 100 - File mode  */
  char uid[8];                  /* 108 - Owner's numeric user ID */
  char gid[8];                  /* 116 - Group's numeric user ID  */
  char size[12];                /* 124  - File size in bytes (octal base) */ 
  char mtime[12];               /* 136 - Last modification time in numeric Unix time format (octal) */
  char chksum[8];               /* 148 - Checksum for header record */
  char typeflag;                /* 156 - Type Flag; Examples: Directory: '5', File:'0' */
  char linkname[100];           /* 157 - Name of linked file  */
  char magic[6];                /* 257 - UStar indicator "ustar" then NULL */
  char version[2];              /* 263 - UStar version "00" */
  char uname[32];               /* 265 - Owner user name */
  char gname[32];               /* 297 - Owner group name */
  char devmajor[8];             /* 329 - Device major number */
  char devminor[8];             /* 337 - Device minor number */
  char prefix[155];             /* 345 - Filename prefix */
                                /* 500 */

	} __packed;
}

/**
 * Reads the contents of the file into the buffer, from the specified file offset.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @param off The offset within the file.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::pread(void* buffer, size_t size, off_t off)
{
	if (off >= this->size()) return 0;
	
	// buffer is a pointer to the buffer that should receive the data.
	// size is the amount of data to read from the file.
	// off is the zero-based offset within the file to start reading from.

	//number of bytes in a block
	size_t block_size = _owner.block_device().block_size();
	//The size of file to be processed
	auto file_size_to_process = this->size();

	//Optimisation: If the size of the buffer is 0 or the file is empty, then simply return that 0 bytes will be read
	if(size == 0 || file_size_to_process == 0) return 0;
	
	//Convert the void type of the buffer into uint8_t and store a pointer to it in ubuffer
	uint8_t * ubuffer = (uint8_t *) buffer;

	//Initialise a temporary different buffer to store the blocks to be read
	uint8_t *current_data_block = new uint8_t[block_size];


	//Given a file and the offset within the file, determine the corresponding block to read from. 
	//i.e) If a file contains data blocks numbered A B C of size  512 bytes each (ignoring the header because _file_start_block starts after the header block)
	//we determine which block corresponds to the offset bytes given to start reading from
	auto block_index = (unsigned int) (off/block_size);

	//Read the blocks from the archive into the temporary current_data_block
	//The archive is read depending on where the file is stored in the archive (set by _file_start_block) in addition to starting_block found previously, which relates to the offset to start reading from
	_owner.block_device().read_blocks(current_data_block, (this->_file_start_block)+block_index, 1);

	//index refers to the block index which we would read from the file inside archive. start index is first determined by the offset
	int index_file = off%block_size;
	//index refers to the buffer that would recieve the data
	int index_buffer=0;
	//stores number of bytes that have been read so far (moved from archive to buffer)
	int bytes_read=0;

	//Retrieve the number of blocks the current file has to make sure we dont read over the limit (even if we dont reach the size of the buffer limit)
	auto number_of_file_blocks = _owner.get_number_of_data_blocks(this->_hdr, block_size);
	//Read while storing things in buffer until the bytes read reaches the size of the buffer
	while(bytes_read<size){
		//Store the value found in the data block into buffer
		ubuffer[index_buffer] = current_data_block[index_file];
		//Increment the values for next iteration
		index_file++;
		index_buffer++;
		//Increment number of bytes read
		bytes_read++;
		//If the current index refering to current_data_block from the archive is divisble by the size of the block, that means we have reached the end of that block
		//In that case, we must proceed to the next block
		if (index_file%block_size == 0){
			//Reset index_file to 0 to start from the beginning of that block
			index_file=0;
			//Next block to read from
			block_index++;
			//In case the next block is not part of the current file, break from the loop as we should not be reading the next file
			if(block_index >= number_of_file_blocks){
				break;
			}
			//We must read from archive the next block into current_data_block in order to access it and store it in buffer. 
			_owner.block_device().read_blocks(current_data_block,(this->_file_start_block)+block_index,1);
		}
	}

	//Make sure that the block number does not exceed the amount of blocks the current file has.
	assert(block_index < number_of_file_blocks);

	return bytes_read;
}

/**
 * Reads all the file headers in the TAR file, and builds an in-memory
 * representation.
 * @return Returns the root TarFSNode that corresponds to the TAR file structure.
 */
TarFSNode* TarFS::build_tree()
{
	// Create the root node.
	TarFSNode *root = new TarFSNode(NULL, "", *this);
	// You must read the TAR file, and build a tree of TarFSNodes that represents each file present in the archive.
	
	//Get the total number of blocks in the archive
	size_t nr_blocks = block_device().block_count();
	//Get the size of one block in bytes
	size_t block_size = block_device().block_size();


	//Define a parent; point to root first
	TarFSNode *parent_node = root;

	//Define the number of blocks to skip over to reach the next header. Will be changed depending on number of blocks current file/directory has
	int block_increment = 0;

	for (int block_index=0;block_index<nr_blocks; block_index+=block_increment){
		//If the next two blocks are zero blocks, that means we have reached the end of archive
		if (check_end_of_archive(block_index,nr_blocks,block_size)){
			break;
		}

		//Store header information by reading the archive
		auto header =(struct posix_header *) new char[block_size];
		block_device().read_blocks(header,block_index,1);

		//Get the number of blocks the current file/directory uses in an archive
		int num_of_data_blocks = get_number_of_data_blocks(header,block_size);


		//Reset and point the parent node to go back to the root
		parent_node = root;
		//From the path name of the file/directory, split it into a list of strings
		String path_to_target = String(header->name);	
		List<String> pathing = path_to_target.split('/', true);


		//current index to traverse in pathing
		int index = 0;
		//If any, get the number of elements to traverse to (i.e if the header gives us the filename docs/README, then count would be two)
		int nodes_count = pathing.count();
		//Given the name, iterate through all the files/directories in the tree in order to build our tree.
		//If while traversing there is no existing child corresponding to the node in the tree, we will add it
		while(index < nodes_count) {
			//If the current child does not exist under the parent node
			if(!parent_node->get_child(pathing.at(index))) {
				//Add a new node with the corresponding name under the current parent node
				TarFSNode *child = new TarFSNode(parent_node, (pathing.at(index)), *this);
				parent_node->add_child(pathing.at(index),child);

				//If the last node is not a directory and we have reached the end of the pathing name, mark it as a file by setting the block offset
				if (header->typeflag!=DIRECTORY_FLAG && nodes_count == index + 1 ){
					//Sets where the block header of the file is 
					child->set_block_offset(block_index);
					child->size(octal2ui(header->size));

				}				
				index++;
			}
			else { 	
				//Get to the next node given by the path name
				parent_node = (TarFSNode*) (parent_node->get_child(pathing.at(index))); 	
				index++;
			}
		}
		//Adjust increment size of the huge loop to go to the next header corresponding to the next file/directory to add to the tree
		block_increment=num_of_data_blocks + 1;

		// Delete the header structure of the current file/directory
		delete header;
	
	}

	return root;
}


/**
 * Given information on the size of the file, get the number of blocks a file/directory uses to store the data, apart from the header.
 * @param header The header structure of the file/directory to get size information from
 * @param block_size The size of each block in archive
 * @return the number of blocks corresponding to the size of the file after the header
 **/
int TarFS::get_number_of_data_blocks(posix_header* header, size_t block_size){
	unsigned int num = 0;
	unsigned int file_size = octal2ui(header->size);
	if (file_size % block_size != 0){
		num+=1;
	}
	num+= (unsigned int) (file_size/block_size);
	return num;
}


/**
 * Determine whether we have reached the end of archive by looking if the next two blocks are zero blocks 
 * @param block_index The current block we are looking at in the archive
 * @param nr_blocks Number of blocks in the archive
 * @param block_size The size of each block in archive
 * @return True if the next two blocks are zero blocks
 **/
bool TarFS::check_end_of_archive(int block_index, size_t nr_blocks, size_t block_size){
	if (block_index<nr_blocks){		
		//Read the next two blocks
		auto temp= new uint8_t[block_size * 2];
		block_device().read_blocks(temp,block_index,2);
		//Determine whether the temp buffer are all zeros
		bool result = (is_zero_block(temp,block_size*2));
		delete temp;
		return result; 
	}
	return false;
}


/**
 * Returns the size of this TarFS File
 */
unsigned int TarFSFile::size() const
{
	// TO BE FILLED IN
	//
	return octal2ui(_hdr->size);

}

/* --- YOU DO NOT NEED TO CHANGE ANYTHING BELOW THIS LINE --- */

/**
 * Mounts a TARFS filesystem, by pre-building the file system tree in memory.
 * @return Returns the root node of the TARFS filesystem.
 */
PFSNode *TarFS::mount()
{
	// If the root node has not been generated, then build it.
	if (_root_node == NULL) {
		_root_node = build_tree();
	}

	// Return the root node.
	return _root_node;
}

/**
 * Constructs a TarFS File object, given the owning file system and the block
 */
TarFSFile::TarFSFile(TarFS& owner, unsigned int file_header_block)
: _hdr(NULL),
_owner(owner),
_file_start_block(file_header_block),
_cur_pos(0)
{
	// Allocate storage for the header.
	_hdr = (struct posix_header *) new char[_owner.block_device().block_size()];
	
	// Read the header block into the header structure.
	_owner.block_device().read_blocks(_hdr, _file_start_block, 1);
	
	// Increment the starting block for file data.
	_file_start_block++;
}

TarFSFile::~TarFSFile()
{
	// Delete the header structure that was allocated in the constructor.
	delete _hdr;
}

/**
 * Releases any resources associated with this file.
 */
void TarFSFile::close()
{
	// Nothing to release.
}

/**
 * Reads the contents of the file into the buffer, from the current file offset.
 * The current file offset is advanced by the number of bytes read.
 * @param buffer The buffer to read the data into.
 * @param size The size of the buffer, and hence the number of bytes to read.
 * @return Returns the number of bytes read into the buffer.
 */
int TarFSFile::read(void* buffer, size_t size)
{
	// Read can be seen as a special case of pread, that uses an internal
	// current position indicator, so just delegate actual processing to
	// pread, and update internal state accordingly.

	// Perform the read from the current file position.
	int rc = pread(buffer, size, _cur_pos);

	// Increment the current file position by the number of bytes that was read.
	// The number of bytes actually read may be less than 'size', so it's important
	// we only advance the current position by the actual number of bytes read.
	_cur_pos += rc;

	// Return the number of bytes read.
	return rc;
}

/**
 * Moves the current file pointer, based on the input arguments.
 * @param offset The offset to move the file pointer either 'to' or 'by', depending
 * on the value of type.
 * @param type The type of movement to make.  An absolute movement moves the
 * current file pointer directly to 'offset'.  A relative movement increments
 * the file pointer by 'offset' amount.
 */
void TarFSFile::seek(off_t offset, SeekType type)
{
	// If this is an absolute seek, then set the current file position
	// to the given offset (subject to the file size).  There should
	// probably be a way to return an error if the offset was out of bounds.
	if (type == File::SeekAbsolute) {
		_cur_pos = offset;
	} else if (type == File::SeekRelative) {
		_cur_pos += offset;
	}
	if (_cur_pos >= size()) {
		_cur_pos = size() - 1;
	}
}

TarFSNode::TarFSNode(TarFSNode *parent, const String& name, TarFS& owner) : PFSNode(parent, owner), _name(name), _size(0), _has_block_offset(false), _block_offset(0)
{
}

TarFSNode::~TarFSNode()
{
}

/**
 * Opens this node for file operations.
 * @return 
 */
File* TarFSNode::open()
{

	// This is only a file if it has been associated with a block offset.
	if (!_has_block_offset) {
		return NULL;
	}

	// Create a new file object, with a header from this node's block offset.
	return new TarFSFile((TarFS&) owner(), _block_offset);
}

/**
 * Opens this node for directory operations.
 * @return 
 */
Directory* TarFSNode::opendir()
{
	return new TarFSDirectory(*this);
}

/**
 * Attempts to retrieve a child node of the given name.
 * @param name
 * @return 
 */
PFSNode* TarFSNode::get_child(const String& name)
{
	TarFSNode *child;

	// Try to find the given child node in the children map, and return
	// NULL if it wasn't found.
	if (!_children.try_get_value(name.get_hash(), child)) {
		return NULL;
	}

	return child;
}

/**
 * Creates a subdirectory in this node.  This is a read-only file-system,
 * and so this routine does not need to be implemented.
 * @param name
 * @return 
 */
PFSNode* TarFSNode::mkdir(const String& name)
{
	// DO NOT IMPLEMENT
	return NULL;
}

/**
 * A helper routine that updates this node with the offset of the block
 * that contains the header of the file that this node represents.
 * @param offset The block offset that corresponds to this node.
 */
void TarFSNode::set_block_offset(unsigned int offset)
{
	_has_block_offset = true;
	_block_offset = offset;
}

/**
 * A helper routine that adds a child node to the internal children
 * map of this node.
 * @param name The name of the child node.
 * @param child The actual child node.
 */
void TarFSNode::add_child(const String& name, TarFSNode *child)
{
	_children.add(name.get_hash(), child);
}

TarFSDirectory::TarFSDirectory(TarFSNode& node) : _entries(NULL), _nr_entries(0), _cur_entry(0)
{
	_nr_entries = node.children().count();
	_entries = new DirectoryEntry[_nr_entries];

	int i = 0;
	for (const auto& child : node.children()) {
		_entries[i].name = child.value->name();
		_entries[i++].size = child.value->size();
	}
}

TarFSDirectory::~TarFSDirectory()
{
	delete _entries;
}

bool TarFSDirectory::read_entry(infos::fs::DirectoryEntry& entry)
{
	if (_cur_entry < _nr_entries) {
		entry = _entries[_cur_entry++];
		return true;
	} else {
		return false;
	}
}

void TarFSDirectory::close()
{

}

static Filesystem *tarfs_create(VirtualFilesystem& vfs, Device *dev)
{
	if (!dev->device_class().is(BlockDevice::BlockDeviceClass)) return NULL;
	return new TarFS((BlockDevice &) * dev);
}

RegisterFilesystem(tarfs, tarfs_create);
