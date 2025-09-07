#include <iostream>
#include <stdlib.h>
#include "../Loader/loader.h"
#include <capstone/capstone.h>

int disasm(Binary *bin);

int main(int argc , char**argv){
	Binary bin;
	std::string fname;
	
	if(argc < 2){
		std::cerr <<"Usage "<< argv[0]<<" binaryfile"<<std::endl;
		return -1;
	}
	fname.assign(argv[1]);

	if(load_binary(fname,&bin,Binary::BIN_TYPE_AUTO)<0){
		std::cerr<<"Unable to load binary " << fname << std::endl;
		return -1;
	}

	std::cout << "Loaded binary " <<fname << std::endl;

	if(disasm(&bin) <0){
		std::cerr << "Disassembly failed" << std::endl;
		return -1;
	}

	unload_binary(&bin);

	return 0;
}

int disasm(Binary *bin){
	csh dis;
	cs_insn *insn;
	Section *text;
	size_t n;

	text = bin->getTextSection();
	if(!text){
		fprintf(stderr,"Nothing to dissassemble \n");
		return -1;
	}

	if(cs_open(CS_ARCH_X86, CS_MODE_64, &dis)!=CS_ERR_OK){
		fprintf(stderr,"Failed to initialize capstone disassembler \n");
		return -1;
	}

	n = cs_disasm(dis, text->bytes,text->size,text->vma,0,&insn);
	if (n<=0){
		fprintf(stderr,"Disassembly error \n");
		fprintf(stderr, "Disassembly error: %s\n", cs_strerror(cs_errno(dis)));
		cs_close(&dis);
		return -1;
	}


	printf("Disassembling .text section of our binary of size %zu bytes\n\n", text->size);
	for(size_t i=0; i<n;i++){
		printf("0x%016jx :",insn[i].address);
		for(size_t j=0;j<insn[i].size;j++){
			printf("%02x ",insn[i].bytes[j]);
		}
		printf("%-12s %s\n",insn[i].mnemonic,insn[i].op_str);
	}

	cs_free(insn,n);
	cs_close(&dis);
	return 0;
}