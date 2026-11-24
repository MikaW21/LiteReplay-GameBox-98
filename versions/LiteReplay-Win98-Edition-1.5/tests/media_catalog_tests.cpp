#include "media_catalog.hpp"
#include <cassert>
#include <iostream>
int main(){
 using namespace lr;MediaCatalog c;
 MediaAsset a;a.path="G:\\游戏 clips\\a\"b.mp4";a.key="g:/unique";a.date="2026-11-09";a.type="mp4";a.bytes=100;a.stamp=123;
 auto id=c.add(a);assert(id==1);a.bytes=200;assert(c.add(a)==id&&c.rows().size()==1);
 MediaCatalog restored;restored.load(c.serialize());assert(restored.rows().at(0).path==a.path&&restored.rows()[0].bytes==200&&restored.rows()[0].stamp==123);
 auto baseline=restored.serialize();
 for(auto bad:{std::string("LITEMEDIA 99 1 0\n"),std::string("LITEMEDIA 1 1 10001\n"),baseline+"trailing",baseline.substr(0,baseline.size()/2),std::string("LITEMEDIA 1 0 0\n")}){
  bool rejected=false;try{restored.load(bad);}catch(const std::exception&){rejected=true;}assert(rejected&&restored.serialize()==baseline);
 }
 a.path="other.mov";a.key="other";a.type="mov";auto second=c.add(a);c.remove(id);assert(!c.find(id)&&c.find(second));
 auto after=c.serialize();restored.load(after);a.key="third";auto third=restored.add(a);assert(third>second);
 MediaCatalog bounded;for(size_t i=0;i<MediaCatalog::limit;++i){a.key=std::to_string(i);bounded.add(a);}bool rejected=false;a.key="overflow";try{bounded.add(a);}catch(const std::exception&){rejected=true;}assert(rejected&&bounded.rows().size()==MediaCatalog::limit);
 // Invalid duplicate records must not replace the live index.
 rejected=false;try{restored.load("LITEMEDIA 1 3 2\n1 1 0 \"a\" \"key\" \"2026\" \"mp4\"\n2 1 0 \"b\" \"key\" \"2026\" \"mp4\"\n");}catch(const std::exception&){rejected=true;}assert(rejected);
 std::cout<<"PASS: stable identity, deduplication, Unicode/quote roundtrip, metadata, corruption rollback, limits and deletion\n";
}
