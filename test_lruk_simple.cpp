#include <iostream>
#include "buffer/lru_k_replacer.h"

int main() {
    std::cout << "Testing LRU-K Replacer with current implementation..." << std::endl;
    
    bustub::LRUKReplacer replacer(7, 2);
    
    // Add six frames to the replacer
    replacer.RecordAccess(1);
    replacer.RecordAccess(2);
    replacer.RecordAccess(3);
    replacer.RecordAccess(4);
    replacer.RecordAccess(5);
    replacer.RecordAccess(6);
    
    replacer.SetEvictable(1, true);
    replacer.SetEvictable(2, true);
    replacer.SetEvictable(3, true);
    replacer.SetEvictable(4, true);
    replacer.SetEvictable(5, true);
    replacer.SetEvictable(6, false);  // Frame 6 is non-evictable
    
    std::cout << "Size: " << replacer.Size() << " (expected: 5)" << std::endl;
    
    // Test eviction
    auto frame = replacer.Evict();
    if (frame.has_value()) {
        std::cout << "Evicted frame: " << frame.value() << std::endl;
    } else {
        std::cout << "No frame evicted" << std::endl;
    }
    
    std::cout << "Size after eviction: " << replacer.Size() << std::endl;
    
    std::cout << "Simple test completed!" << std::endl;
    return 0;
}
