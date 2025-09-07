#include<bfd.h>
#include "loader.h"
#include<iostream>

// Forward declarations
static bfd* open_bfd(std::string &fname);
static int load_binary_bfd(std::string &fname, Binary *bin, Binary::BinaryType type);
static int load_symbols_bfd(bfd *bfd_h, Binary *bin);
static int load_dynsym_bfd(bfd *bfd_h, Binary *bin);
static int load_sections_bfd(bfd *bfd_h, Binary *bin);

int load_binary(std::string &fname,Binary *bin,Binary::BinaryType type){
    return load_binary_bfd(fname,bin,type);
}

void unload_binary(Binary *bin){
    size_t i;
    Section *sec;

    for(i=0; i<bin->sections.size();i++){
        sec = &bin->sections[i];
        if(sec->bytes){
            free(sec->bytes);
        }
    }
}

static bfd* open_bfd(std::string &fname){
    static int bfd_inited =0;
    bfd *bfd_h;

    if(!bfd_inited){
        bfd_init();
        bfd_inited =1;
    }

    bfd_h =bfd_openr(fname.c_str(),NULL);
    if(!bfd_h){
        fprintf(stderr, "failed to open binary '%s' (%s)\n",fname.c_str(),bfd_errmsg(bfd_get_error()));
        return NULL;
    }

    if(!bfd_check_format(bfd_h, bfd_object)){
        fprintf(stderr, "file '%s' doesn't look like an executable (%s)\n", fname.c_str(), bfd_errmsg(bfd_get_error()));
        bfd_close(bfd_h);
        return NULL;
    }

    /**
        Some versions of bfd_check_format pessimistically set a wrong format
     */

    bfd_set_error(bfd_error_no_error);

    if(bfd_get_flavour(bfd_h)==bfd_target_unknown_flavour){
        fprintf(stderr,"unrecognized format for binary %s  (%s)",fname.c_str(),bfd_errmsg(bfd_get_error()));
        return NULL;
    }

return bfd_h;

}


static int load_binary_bfd(std::string &fname, Binary *bin,Binary::BinaryType type){
    int ret;
    bfd *bfd_h;
    const bfd_arch_info_type *bfd_info;

    bfd_h = NULL;
    //It starts by copying the name of the binary file and using libffd to find and copy the entry point address.
    bfd_h = open_bfd(fname); //This will give load_binary_bfd functions some of bin's basic properties
    if(!bfd_h){
        goto fail;
    }

    bin->filename =std::string(fname);
    bin->entry = bfd_get_start_address(bfd_h); //this gives the entry point address. It returns the valuse of the start_address field
//The start address is nothing but bfd_vma a 64 bit unsigned integer.


//Loader needs to know what is the binary type .
//For this we need to know about the bfd_target struct maintained by the lfbffd
//to get a pointer to that data structure we need a pointer 
//bfd_h->xvec gives us that pointer to a bfd_target_structure
    bin->type_str =std::string(bfd_h->xvec->name);
    switch(bfd_h->xvec->flavour){
        case bfd_target_elf_flavour:
            bin->type = Binary::BIN_TYPE_ELF;
            break;
        case bfd_target_coff_flavour:
            bin->type = Binary::BIN_TYPE_PE;
            break;
        case bfd_target_unknown_flavour:
            default:
                fprintf(stderr, "unsupported binary type (%s)\n",bfd_h->xvec->name);
                goto fail;
    }



//Now we know what binary it is whether elf or PE
//But we still do not know about the architecture
//To find out about the architecture , libffd has one function called as bfd_get_arch_info
//This function returns a pointer to the data structure which provides information about the binary architecture
//So this provides a convenient printable string describing the architecture, which the loader copies into the Binary Object
    bfd_info = bfd_get_arch_info(bfd_h);
    bin->arch_str = std::string(bfd_info->printable_name);
    switch(bfd_info->mach){ //mach is the short form of machine which is an integer identifier for the architecture
        case bfd_mach_i386_i386:
            bin->arch = Binary::ARCH_X86;
            bin->bits = 32;
            break;
        case bfd_mach_x86_64:
            bin->arch = Binary::ARCH_X86;
            bin->bits = 64;
            break;
        default:
            fprintf(stderr, "unsupported architecture (%s)\n",bfd_info->printable_name);
            goto fail;
    }

    load_symbols_bfd(bfd_h,bin);
    load_dynsym_bfd(bfd_h,bin);

    if(load_sections_bfd(bfd_h,bin)<0)goto fail;
    ret =0;
    goto cleanup;

    fail:
        ret =-1;
    
    cleanup:
        if(bfd_h){
            std::cout<< "Closing bfd handle"<<std::endl;    
            bfd_close(bfd_h);
        }
        return ret;

}


static int load_symbols_bfd (bfd *bfd_h, Binary *bin){
    int ret;
    long n, nsyms, i;
    asymbol **bfd_symtab; //In libffd, symbols are represented by the asymbol structure.Double pointer here means that an array of pointers to symbols.
    Symbol *sym;

    /*
        So the main function of this load_symbols_bfd function is to read the symbol table and copy the symbol names and addresses into the Binary Object
    */

    bfd_symtab = NULL;

    n = bfd_get_symtab_upper_bound(bfd_h); //This tells us how many bytes are needed to be allocated.Negative in case of an error
    if(n<0){
        fprintf(stderr, "failed to read symtab (%s)\n", bfd_errmsg(bfd_get_error()));
        goto fail;
    }
    else if(n){
        bfd_symtab = (asymbol**) malloc(n);
        if(!bfd_symtab){
            fprintf(stderr, "out of memory\n");
            goto fail;
        }
        nsyms = bfd_canonicalize_symtab(bfd_h, bfd_symtab);//To populate symbols,needs bfd handle and symbol table we want to populate
        if(nsyms<0){
            fprintf(stderr, "failed to read symtab (%s)\n", bfd_errmsg(bfd_get_error()));
            goto fail;
        }
        for(i=0; i<nsyms; i++){
            if(bfd_symtab[i]->flags & BSF_FUNCTION){ //BSF_FUNCTION is a flag telling that that symbol is for a function
                bin->symbols.push_back(Symbol());
                sym = &bin->symbols.back();
                sym->type = Symbol::SYM_TYPE_FUNC;
                sym->name = std::string(bfd_symtab[i]->name);
                sym->addr = bfd_asymbol_value(bfd_symtab[i]);
            }
        }
        }

        ret =0;
        goto cleanup;

        fail:
            ret =-1;
        cleanup:
            if(bfd_symtab) free(bfd_symtab);
            return ret;
}

static int load_dynsym_bfd(bfd *bfd_h, Binary* bin){
    int ret;
    long n, nsyms, i;
    asymbol **bfd_dynsym;
    Symbol *sym;

    bfd_dynsym = NULL;

    n = bfd_get_dynamic_symtab_upper_bound(bfd_h);
    if(n<0){
        fprintf(stderr, "failed to read dynamic symtab (%s)\n", bfd_errmsg(bfd_get_error()));
        goto fail;
    }
    else if(n){
        bfd_dynsym = (asymbol**) malloc(n);
        if(!bfd_dynsym){
            fprintf(stderr, "out of memory\n");
            goto fail;
        }
        nsyms = bfd_canonicalize_dynamic_symtab(bfd_h, bfd_dynsym);
        if(nsyms<0){
            fprintf(stderr, "failed to read dynamic symtab (%s)\n", bfd_errmsg(bfd_get_error()));
            goto fail;
        }
        for(i=0; i<nsyms; i++){
            if(bfd_dynsym[i]->flags & BSF_FUNCTION){
                bin->symbols.push_back(Symbol());
                sym = &bin->symbols.back();
                sym->type = Symbol::SYM_TYPE_FUNC;
                sym->name = std::string(bfd_dynsym[i]->name);
                sym->addr = bfd_asymbol_value(bfd_dynsym[i]);
            }
        }
    }
    ret =0;
    goto cleanup;

    fail:
        ret =-1;
    cleanup:
            if(bfd_dynsym) free(bfd_dynsym);
            return ret;
}

//Loading sections

static int load_sections_bfd(bfd *bfd_h, Binary *bin){
    int bfd_flags;
    uint64_t vma,size;
    const char *secname;
    asection* bfd_sec; //To store sections in the binary object, we need to know about the bfd_section structure
    /*
        Internally, libffd maintains a linked list of sections, where each section has a pointer to the next section in the list.
        The bfd_get_section_by_name function returns a pointer to the first section in the list that matches the given name.
    */
    int ret;
    Section *sec;
    Section::SectionType sectype;

    for(bfd_sec =bfd_h->sections;bfd_sec;bfd_sec=bfd_sec->next){
        bfd_flags = bfd_section_flags(bfd_sec);

        sectype = Section::SEC_TYPE_NONE;
        if(bfd_flags & SEC_CODE){
            sectype = Section::SEC_TYPE_CODE;
        }
        else if(bfd_flags & SEC_DATA){
            sectype = Section::SEC_TYPE_DATA;
        }
        else{
            continue;
        }
        vma = bfd_section_vma(bfd_sec);
        size = bfd_section_size(bfd_sec);
        secname = bfd_section_name(bfd_sec);
        if(!secname){
            secname ="<unnamed>";
        }
        bin->sections.push_back(Section());
        sec = &bin->sections.back();

        //sec =&bin->sections.back();

        sec->binary =bin;
        sec->name = std::string(secname);
        sec->type =sectype;
        sec->vma =vma;
        sec->size =size;
        sec->bytes =(uint8_t*) malloc(size);
        if(!sec->bytes){
            fprintf(stderr, "out of memory\n");
            goto fail;
        }
        if(!bfd_get_section_contents(bfd_h, bfd_sec, sec->bytes, 0, size)){
            fprintf(stderr, "failed to read section '%s' (%s)\n", secname, bfd_errmsg(bfd_get_error()));
            goto fail;
        }
    }
        
    

        ret =0;
        goto cleanup;

        fail:
            return -1;
        
        cleanup:
                return ret;
        
        return 0;
    }