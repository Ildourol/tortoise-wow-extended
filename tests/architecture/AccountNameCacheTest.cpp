#include <map>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>
using uint32=unsigned;
constexpr unsigned MAX_ACCOUNT_STR=16;
enum AccountOpResult{AOR_OK,AOR_NAME_NOT_EXIST,AOR_PASS_TOO_LONG,AOR_DB_INTERNAL_ERROR};
bool normalizeString(std::string& s){if(s=="!invalid!")return false;for(char& c:s)c=std::toupper((unsigned char)c);return true;}
unsigned utf8length(std::string const& s){return unsigned(s.size());}
std::string hashName;
std::string CalculateShaPassHash(std::string name,std::string){hashName=name;return "mock-hash";}
struct Field {std::string name;std::string GetCppString(){return name;}};
struct QueryResult {Field field;Field& operator[](unsigned){return field;}};
struct Database {
    std::map<unsigned,std::string> names;unsigned reads=0,writes=0;bool writeOK=true;
    QueryResult* PQuery(char const*,unsigned id){++reads;auto i=names.find(id);return i==names.end()?nullptr:new QueryResult{{i->second}};}
    bool PExecute(char const*,char const*,unsigned){++writes;return writeOK;}
}LoginDatabase;
struct AccountMgr {
    struct Data {std::string Username;};std::map<unsigned,Data> m_accountData;
    bool GetName(unsigned,std::string&);
    AccountOpResult ChangePassword(unsigned,std::string,std::string="");
};
#include "AccountNameNative.inc"
#include "AccountPasswordNative.inc"
void Check(bool ok,char const* why){if(!ok)throw std::runtime_error(why);}
int main(){
    AccountMgr mgr;LoginDatabase.names[1]="testuser";mgr.m_accountData[1]={};
    Check(mgr.ChangePassword(1,"testpass")==AOR_OK && hashName=="TESTUSER","partial cache hashed empty username");
    Check(LoginDatabase.reads==1 && LoginDatabase.writes==1,"native fallback/write missing");
    mgr.m_accountData[1].Username="CACHED";
    Check(mgr.ChangePassword(1,"testpass")==AOR_OK && hashName=="CACHED" && LoginDatabase.reads==1,"populated cache ignored");
    mgr.m_accountData[2]={};
    Check(mgr.ChangePassword(2,"testpass")==AOR_NAME_NOT_EXIST && LoginDatabase.writes==2,"missing account wrote password");
    Check(mgr.ChangePassword(1,"testpass","sessionname")==AOR_OK && hashName=="SESSIONNAME","explicit session username changed");
    Check(mgr.ChangePassword(1,std::string(17,'x'))==AOR_PASS_TOO_LONG && LoginDatabase.writes==3,"length check bypassed");
    LoginDatabase.writeOK=false;
    Check(mgr.ChangePassword(1,"testpass")==AOR_DB_INTERNAL_ERROR,"write failure concealed");
    std::cout<<"Native account lookup and password-change cache cases passed\n";
}
