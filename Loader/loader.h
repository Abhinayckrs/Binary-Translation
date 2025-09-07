#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>
#include <string>
#include <vector>

class Symbol;
class Binary;
class Section {
public:
    enum SectionType {
        SEC_TYPE_NONE = 0,
        SEC_TYPE_CODE = 1,
        SEC_TYPE_DATA = 2
    };

    Section() : binary(NULL), type(SEC_TYPE_NONE), vma(0), size(0), bytes(NULL) {}

    bool contains(uint64_t addr) {
        return (addr >= vma) && (addr - vma < size);
    }

    Binary *binary;  // Pointer to the Binary object this Section belongs to
    std::string name;  // Name of the section (e.g., ".text", ".data")
    SectionType type;  // Type of section (e.g., code, data)
    uint64_t vma;  // Virtual memory address
    uint64_t size;  // Size of the section
    uint8_t *bytes;  // Raw bytes of the section (if applicable)
};

// Symbol class definition (no changes needed here)
class Symbol {
public:
    enum SymbolType {
        SYM_TYPE_UKN = 0,
        SYM_TYPE_FUNC = 1,
    };

    Symbol() : type(SYM_TYPE_UKN), name(), addr(0) {}

    SymbolType type;  // Type of the symbol (unknown, function, etc.)
    std::string name;  // Name of the symbol
    uint64_t addr;  // Address of the symbol (e.g., function address)
};

// Binary class definition with the full definition of Section
class Binary {
public:
    enum BinaryType {
        BIN_TYPE_AUTO = 0,
        BIN_TYPE_ELF = 1,
        BIN_TYPE_PE = 2,
    };

    enum BinaryArch {
        ARCH_NONE = 0,
        ARCH_X86 = 1,
    };

    Binary() : type(BIN_TYPE_AUTO), arch(ARCH_NONE), bits(0), entry(0) {}

    // Function to get the ".text" section (code section)
    Section *getTextSection() {
        for (auto &s : sections) {
            if (s.name == ".text") {  // Look for the ".text" section
                return &s;  // Return pointer to the section
            }
        }
        return NULL;  // If not found, return NULL
    }

    std::string filename;  // Name of the binary file
    BinaryType type;  // Type of binary (e.g., ELF, PE)
    BinaryArch arch;  // Architecture type (e.g., x86)
    std::string type_str;  // String representation of binary type
    std::string arch_str;  // String representation of architecture
    unsigned bits;  // Number of bits (e.g., 32-bit or 64-bit)
    uint64_t entry;  // Entry point address of the binary
    std::vector<Section> sections;  // List of sections in the binary
    std::vector<Symbol> symbols;  // List of symbols in the binary
};

// Function declarations
int load_binary(std::string &fname, Binary *bin, Binary::BinaryType type);
void unload_binary(Binary *bin);

#endif // LOADER_H
