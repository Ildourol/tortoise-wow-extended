#include <stdexcept>
#include "NativeBagEnums.inc"
#include "NativeQuiverEnums.inc"
struct ItemPrototype { unsigned Class=0,SubClass=0,BagFamily=0; };
#include "NativeBagStorage.inc"
void check(bool value) { if(!value) throw std::runtime_error("bag storage failed"); }
int main() {
    // Expected client family IDs, including the four Turtle specialty bags.
    unsigned const familyForSubclass[]={0,3,6,7,8,99,13,12,10,11};
    ItemPrototype bag; bag.Class=ITEM_CLASS_CONTAINER; ItemPrototype item;
    for(unsigned subclass=0;subclass<10;++subclass) {
        bag.SubClass=subclass;
        for(unsigned family=0;family<=13;++family) {
            item.BagFamily=family;
            check(ItemCanGoIntoBag(&item,&bag)==(subclass==0 || family==familyForSubclass[subclass]));
        }
    }
    bag.Class=ITEM_CLASS_QUIVER;
    for(unsigned subtype=2;subtype<=3;++subtype) {
        bag.SubClass=subtype;
        for(unsigned family=0;family<=13;++family) {
            item.BagFamily=family;
            check(ItemCanGoIntoBag(&item,&bag)==(family==subtype-1));
        }
    }
    check(!ItemCanGoIntoBag(nullptr,&bag)); check(!ItemCanGoIntoBag(&item,nullptr));
    bag.Class=ITEM_CLASS_WEAPON; check(!ItemCanGoIntoBag(&item,&bag));
    bag.Class=ITEM_CLASS_CONTAINER;bag.SubClass=50;check(!ItemCanGoIntoBag(&item,&bag));
}
