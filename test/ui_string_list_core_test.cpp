#include "../hook/ui_string_list_core.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <stdexcept>

int main() {
    bool bad_dimensions = false;
    try { ui_core::OwnedStringList invalid(1, 0); }
    catch (const std::invalid_argument&) { bad_dimensions = true; }
    assert(bad_dimensions);

    ui_core::OwnedStringList list(4, 6);
    assert(list.capacity() == 4 && list.count() == 0 && list.changes() == 0);
    assert(list.get(0) == nullptr);
    assert(list.add(""));
    assert(list.count() == 1 && list.changes() == 1 && list.get(0)[0] == 0);

    assert(list.add("alpha-long"));
    char* stable_entry = list.get(1);
    assert(list.add("beta"));
    assert(list.add("gamma"));
    assert(list.count() == 4 && list.changes() == 4);
    assert(list.get(1) == stable_entry); // adding entries does not move existing buffers
    assert(std::strcmp(stable_entry, "alpha") == 0); // fixed-size entry is always terminated
    assert(std::strcmp(list.get(2), "beta") == 0);
    assert(list.get(-1) == nullptr);
    assert(list.get(4) == nullptr);

    assert(!list.add("overflow"));
    assert(list.count() == 4 && list.changes() == 4);
    assert(!list.erase(-1));
    assert(!list.erase(4));
    assert(list.count() == 4 && list.changes() == 4);

    assert(list.erase(2));
    assert(list.count() == 3 && list.changes() == 5);
    assert(std::strcmp(list.get(2), "gamma") == 0);
    assert(list.erase(2));
    assert(list.count() == 2 && list.changes() == 6);
    assert(std::strcmp(list.get(1), "alpha") == 0);

    std::cout << "native owned UI string-list tests passed (host pointers: " << sizeof(void*) * 8 << " bits)\n";
}
