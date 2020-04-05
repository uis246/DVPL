#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>

#include <potato.h>

#include <lz4.h>
#include <lz4hc.h>

#define UnloadFile(ptr, size) unloadFile(ptr, size)
__inline void unloadFile(void* ptr, size_t size){
    munmap(ptr, size);
}

static struct argp_option options[] = {
    { "pack", 'p', "type", OPTION_ARG_OPTIONAL, "Pack DVPL"},
    { "unpack", 'u', 0, 0, "Unpack DVPL"},
//    { "verbose", 'v', 0, 0, "Verbose"},
    { 0 }
};
static char doc[] = "DVPL (un)packer";
static char args_doc[] = "FILENAME";
char* fname=0;
static struct __attribute((packed)){
    bool pack;
    bool unpack;
//    bool verbose;
    __attribute((aligned(4))) int type;
}args={false,false,false};
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case 'p': if(!args.unpack){
            args.pack=true;
            if(arg&&strlen(arg)==1){
                if(arg[0]>='0'&&arg[0]<='9'){
                    args.type=arg[0]-'0';
                }else{args.type=2;}
            }else{
                args.type=2;
            }
            break;
        }else
            return ARGP_ERR_UNKNOWN;
        case 'u': if(!args.pack){
            args.unpack=true;
            break;
        }else
            return ARGP_ERR_UNKNOWN;
//        case 'v': args.verbose=true; break;
        case ARGP_KEY_ARG:
            fname=arg;
            return 0;
        default: return ARGP_ERR_UNKNOWN;
    }
    return 0;
}
static struct argp argp = { options, parse_opt, args_doc, doc, 0, 0, 0 };


/*uint32_t crc32b(unsigned char *message, size_t size) {
   int i, j;
   uint32_t byte, crc, mask;

   crc = 0xFFFFFFFF;
   for(size_t i=0; i<size; i++){
      byte = message[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
         mask = -(crc & 1);
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
   }
   return ~crc;
}*/


int main(int argc, char* argv[])
{
    argp_parse(&argp, argc, argv, 0, 0, 0/*&args*/);

    if(!(args.pack||args.unpack)){
        dprintf(2, "Specefy -p or -u\n");
		argp_help(&argp, stderr, ARGP_HELP_SEE, argv[0]);
		exit(0);
    }

    if(!fname){
        dprintf(2, "No file present\n");
        abort();
    }

    FILE* f;
    if(!(f=fopen(fname, "rb"))){
        dprintf(2, "File does not exists\n");
        abort();
    }
    fseek(f, 0L, SEEK_END);
    size_t size = ftell(f);
//    fseek(f, 0L, SEEK_SET);
    char* p=mmap(NULL, size, PROT_READ, MAP_SHARED, fileno(f), 0);
	fclose(f);

	size_t sz;
	int fd2;
	char* data;

	if(args.unpack){
		if((sz=dvpl_check(p, size))==0){
			dprintf(2, "Bad DVPL\n");
			abort();
		}

		data=strrchr(fname, '.');
		if(data&&(fd2=strlen(fname))>5&&!strcmp(data, ".dvpl")){
			fname[fd2-5]=0;
		}else{
			fname="orig";
		}

		if(!(fd2=open(fname, O_RDWR|O_CREAT, 0666))){
			dprintf(2, "Fopen error\n");
			abort();
		}
		ftruncate(fd2, sz);
		if((data=mmap(NULL, sz, PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0))==-1){
			perror("Mmap");
			abort();
		}
		if(dvpl_unpack(p, size, data)==LIBPOTATO_NOT_IMPLEMENTED){
			dprintf(2, "Unsupported compression type\n");
			abort();
		}
	}else{
		char* fn;
		{
			size_t sz=strlen(fname);
			fn=malloc(sz+6);
			memcpy(fn, fname, sz);
			memcpy(fn+sz, ".dvpl", 6);
		}

		if(!(fd2=open(fn, O_RDWR|O_CREAT, 0666))){
			dprintf(2, "Fopen error\n");
			abort();
		}

		sz=dvpl_pack_max(size, args.type);

		ftruncate(fd2, sz);
		if((data=mmap(NULL, sz, PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0))==-1){
			perror("Mmap");
			abort();
		}

		if(dvpl_pack(data, &sz, p, size, args.type)==LIBPOTATO_NOT_IMPLEMENTED){
			dprintf(2, "Unsupported compression type\n");
			abort();
		}
		ftruncate(fd2, sz);

		free(fn);
	}
/*
    if(args.unpack){
        struct footer* ft=p+size-sizeof(struct footer);

	if(memcmp(ft->marker, "DVPL", 4)){
            dprintf(2, "This is not DVPL!\n");
            abort();
        }

        if(size-sizeof(struct footer)!=ft->packed_len){
            dprintf(2, "File size missmatch!\n");
            abort();
        }

        uint32_t crc32,rcrc32;
        crc32=crc32b(p, ft->packed_len);
        if(crc32!=ft->crc32){
            dprintf(2, "CRC32 missmatch!\n");
            abort();
        }
//        if(ft->packed_len==0){}

        char *dot = strrchr(fname, '.');
        if(dot && !strcmp(dot, ".dvpl")){
		fname[strlen(fname)-5]=0;
	}else{
		fname="orig";
	}

        int fd2;
        if(!(fd2=open(fname, O_RDWR|O_CREAT, 0666))){
            dprintf(2, "Fopen error\n");
            abort();
        }

        char* u;
        if(ftruncate(fd2, ft->len)){
            perror("Truncate error");
            abort();
        }
        if((u=mmap(NULL, ft->len, PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0))==-1){
            perror("Mmap");
            abort();
        }
        switch(ft->pack_type){
        case 0://RAW
            memcpy(u, p, ft->len);
            break;
        case 1://LZ4
        case 2://LZ4HC
            LZ4_decompress_safe(p, u, ft->packed_len, ft->len);
//            munmap(p, size);
            break;
        case 4://ZIP
            dprintf(2, "Deflate not implemented\n");
            abort();
        default:
            dprintf(2, "Compression type error\n");
            abort();
        }
    }else{
        char* fn;
        {
            size_t sz=strlen(fname);
            fn=malloc(sz+6);
            memcpy(fn, fname, sz);
            memcpy(fn+sz, ".dvpl", 6);
        }
        char* compressed_data;
        size_t compressed_size, max_dst_size;
        
	int fd2;
        if(!(fd2=open(fn, O_RDWR|O_CREAT, 0666))){
            dprintf(2, "Fopen error\n");
            abort();
        }
        switch(args.type) {
        case 0://RAW
            if(ftruncate(fd2, size+sizeof(struct footer))){
                perror("Truncate error");
                abort();
            }
            if((compressed_data=mmap(NULL, size, PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0))==-1){
                perror("Mmap");
                abort();
            }
            compressed_data=size;
            memcpy(compressed_data, p, size);
        case 2://LZ4HC
        {
            max_dst_size=LZ4_compressBound(size);
            if(ftruncate(fd2, max_dst_size)){
                perror("Truncate error");
                abort();
            }
            if((compressed_data=mmap(NULL, max_dst_size, PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0))==-1){
                perror("Mmap");
                abort();
            }
            compressed_size=LZ4_compress_HC(p, compressed_data, size, max_dst_size, LZ4HC_CLEVEL_MAX);
            if(compressed_size<=0){
                dprintf(2, "LZ4 compression fail");
                abort();
            }
//            munmap(compressed_data, max_dst_size);
            ftruncate(fd2, compressed_size+sizeof(struct footer));
//            compressed_data=mmap(NULL, compressed_size+sizeof(struct footer), PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0);
            break;
        }
	case 1://LZ4
		max_dst_size=LZ4_compressBound(size);

		if(ftruncate(fd2, max_dst_size)){
			perror("Truncate error");
			abort();
		}

		if((compressed_data=mmap(NULL, max_dst_size, PROT_WRITE, MAP_SHARED|MAP_FILE, fd2, 0))==-1){
			perror("Mmap");
			abort();
		}

		compressed_size=LZ4_compress_default(p, compressed_data, size, max_dst_size);
		if(compressed_size<=0){
			dprintf(2, "LZ4 compression fail");
			abort();
		}

		ftruncate(fd2, compressed_size+sizeof(struct footer));
		break;
        default:
            dprintf(2, "Compression type %d is unsupported\n", args.type);
            abort();
        }
        struct footer* ft=compressed_data+compressed_size;
        ft->crc32=crc32b(compressed_data, compressed_size);
        ft->packed_len=compressed_size;
        ft->len=size;
        ft->pack_type=2;
        memcpy(ft->marker, "DVPL", 4);
        munmap(compressed_data, compressed_size);
	}
*/
/*if(msync(u, ft->len, MS_SYNC)){
					perror("Msync");
					abort();
}*/
    return 0;
}
