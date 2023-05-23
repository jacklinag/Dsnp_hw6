/****************************************************************************
  FileName     [ cirMgr.cpp ]
  PackageName  [ cir ]
  Synopsis     [ Define cir manager functions ]
  Author       [ Chung-Yang (Ric) Huang ]
  Copyright    [ Copyleft(c) 2008-present LaDs(III), GIEE, NTU, Taiwan ]
****************************************************************************/

#include <iostream>
#include <iomanip>
#include <cstdio>
#include <ctype.h>
#include <cassert>
#include <cstring>
#include "cirMgr.h"
#include "cirGate.h"
#include "util.h"
#include <stdlib.h>

using namespace std;

// TODO: Implement memeber functions for class CirMgr

/*******************************/
/*   Global variable and enum  */
/*******************************/
CirMgr* cirMgr = 0;

enum CirParseError {
   EXTRA_SPACE,
   MISSING_SPACE,
   ILLEGAL_WSPACE,
   ILLEGAL_NUM,
   ILLEGAL_IDENTIFIER,
   ILLEGAL_SYMBOL_TYPE,
   ILLEGAL_SYMBOL_NAME,
   MISSING_NUM,
   MISSING_IDENTIFIER,
   MISSING_NEWLINE,
   MISSING_DEF,
   CANNOT_INVERTED,
   MAX_LIT_ID,
   REDEF_GATE,
   REDEF_SYMBOLIC_NAME,
   REDEF_CONST,
   NUM_TOO_SMALL,
   NUM_TOO_BIG,

   DUMMY_END
};

/**************************************/
/*   Static varaibles and functions   */
/**************************************/
static unsigned lineNo = 0;  // in printint, lineNo needs to ++
static unsigned colNo  = 0;  // in printing, colNo needs to ++
static char buf[1024];
static string errMsg;
static int errInt;
static CirGate *errGate;

static bool
parseError(CirParseError err)
{
   switch (err) {
      case EXTRA_SPACE:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Extra space character is detected!!" << endl;
         break;
      case MISSING_SPACE:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Missing space character!!" << endl;
         break;
      case ILLEGAL_WSPACE: // for non-space white space character
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Illegal white space char(" << errInt
              << ") is detected!!" << endl;
         break;
      case ILLEGAL_NUM:
         cerr << "[ERROR] Line " << lineNo+1 << ": Illegal "
              << errMsg << "!!" << endl;
         break;
      case ILLEGAL_IDENTIFIER:
         cerr << "[ERROR] Line " << lineNo+1 << ": Illegal identifier \""
              << errMsg << "\"!!" << endl;
         break;
      case ILLEGAL_SYMBOL_TYPE:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Illegal symbol type (" << errMsg << ")!!" << endl;
         break;
      case ILLEGAL_SYMBOL_NAME:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Symbolic name contains un-printable char(" << errInt
              << ")!!" << endl;
         break;
      case MISSING_NUM:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Missing " << errMsg << "!!" << endl;
         break;
      case MISSING_IDENTIFIER:
         cerr << "[ERROR] Line " << lineNo+1 << ": Missing \""
              << errMsg << "\"!!" << endl;
         break;
      case MISSING_NEWLINE:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": A new line is expected here!!" << endl;
         break;
      case MISSING_DEF:
         cerr << "[ERROR] Line " << lineNo+1 << ": Missing " << errMsg
              << " definition!!" << endl;
         break;
      case CANNOT_INVERTED:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": " << errMsg << " " << errInt << "(" << errInt/2
              << ") cannot be inverted!!" << endl;
         break;
      case MAX_LIT_ID:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Literal \"" << errInt << "\" exceeds maximum valid ID!!"
              << endl;
         break;
      case REDEF_GATE:
         cerr << "[ERROR] Line " << lineNo+1 << ": Literal \"" << errInt
              << "\" is redefined, previously defined as "
              << errGate->getTypeStr() << " in line " << errGate->getLineNo()
              << "!!" << endl;
         break;
      case REDEF_SYMBOLIC_NAME:
         cerr << "[ERROR] Line " << lineNo+1 << ": Symbolic name for \""
              << errMsg << errInt << "\" is redefined!!" << endl;
         break;
      case REDEF_CONST:
         cerr << "[ERROR] Line " << lineNo+1 << ", Col " << colNo+1
              << ": Cannot redefine constant (" << errInt << ")!!" << endl;
         break;
      case NUM_TOO_SMALL:
         cerr << "[ERROR] Line " << lineNo+1 << ": " << errMsg
              << " is too small (" << errInt << ")!!" << endl;
         break;
      case NUM_TOO_BIG:
         cerr << "[ERROR] Line " << lineNo+1 << ": " << errMsg
              << " is too big (" << errInt << ")!!" << endl;
         break;
      default: break;
   }
   return false;
}

/**************************************************************/
/*   class CirMgr member functions for circuit construction   */
/**************************************************************/
int myId2Num(int, bool);
void resetValue()
{
   lineNo = 0;
   colNo = 0;
   errMsg = "";
   errInt = 0;
   errGate = NULL;
}

bool
CirMgr::readCircuit(const string& fileName)
{  
   fstream inputfile;
   string part;
   vector<int> state; // M I L O A
   vector<vector<int>> PoFanin, AIGFanin;
   inputfile.open(fileName);
   if(!inputfile) {
      cerr << "Cannot open design \"" << fileName << "\"!!" << endl;
      return false;
   }
   resetValue();
   if(!readHeader(inputfile, state)) return false;
   _totalList.resize(state[0]+state[3]+1);
   CirGate* const0 = new Const0();
   _totalList[0] = const0;

   for(int i = 0; i < state[1]; i++){
      if(!readInput(inputfile, state[0])) return false;
   }
   for(int i = 0; i < state[3]; i++){
      if(!readOutput(inputfile, state[0], PoFanin)) return false;
   }
   for(int i = 0; i < state[4]; i++){
      if(!readAig(inputfile, state[0], AIGFanin)) return false;
   }
   setFanIO(PoFanin);
   setFanIO(AIGFanin);
   dfsTraversal(_poList);
   sortFanout();

   bool stopRun = false;
   while(!stopRun){
      if(!readSymbol(inputfile, stopRun)) return false;
   }
   while(readComment(inputfile)){}
   return true;
}

bool
CirMgr::readHeader(fstream& file, vector<int>& state)
{
   bool missNewLine = false;
   string header;
   vector<string> sections;
   getline(file, header);
   if(!cutPiece(header, sections, 6, missNewLine)) return false;
   if(sections.size() == 0){ errMsg = "aag"; return parseError(MISSING_IDENTIFIER); }

   int num;
   if(sections[0] != "aag"){
      string t, si;
      int i;
      t = sections[0].substr(0, 3);
      si = sections[0].substr(3, sections[0].size()-3);
      if(t == "aag" && myStr2Int(si, i)) { colNo = 3; return parseError(MISSING_SPACE); }
      else{ errMsg = sections[0]; return parseError(ILLEGAL_IDENTIFIER); }
   }
   if(sections.size() == 1){ errMsg = "number of variables"; return parseError(MISSING_NUM); }
   else if(sections.size() == 2){ errMsg = "number of PIs"; return parseError(MISSING_NUM); }
   else if(sections.size() == 3){ errMsg = "number of latches"; return parseError(MISSING_NUM); }
   else if(sections.size() == 4){ errMsg = "number of POs"; return parseError(MISSING_NUM); }

   for(int i = 1, s = sections.size(); i < s; i++){
      if(myStr2Int(sections[i], num)){
         if(i == 1 && num < 0){ errMsg = "Number of variables"; errInt = num; return parseError(NUM_TOO_SMALL); }
         else if(i == 2 && num < 0){ errMsg = "Number of PIs"; errInt = num; return parseError(NUM_TOO_SMALL); }
         else if(i == 3 && num < 0){ errMsg = "Number of latches"; errInt = num; return parseError(NUM_TOO_SMALL); }
         else if(i == 4 && num < 0){ errMsg = "Number of POs"; errInt = num; return parseError(NUM_TOO_SMALL); }
         else if(i == 5 && num < 0){ errMsg = "Number of AIGs"; errInt = num; return parseError(NUM_TOO_SMALL); }
         else state.push_back(num);
      }
      else{
         if(i == 1){ errMsg = "number of variables(" + sections[i] + ")"; return parseError(ILLEGAL_NUM); }
         else if(i == 2){ errMsg = "number of PIs(" + sections[i] + ")"; return parseError(ILLEGAL_NUM); }
         else if(i == 3){ errMsg = "number of latches(" + sections[i] + ")"; return parseError(ILLEGAL_NUM); }
         else if(i == 4){ errMsg = "number of POs(" + sections[i] + ")"; return parseError(ILLEGAL_NUM); }
         else if(i == 5){ errMsg = "number of AIGs(" + sections[i] + ")"; return parseError(ILLEGAL_NUM); }
      }
   }   
   if(missNewLine) return parseError(MISSING_NEWLINE);
   int ILA = state[1] + state[2] + state[4];
   if(ILA > state[0]) { errMsg = "Number of variables"; errInt = state[0]; return parseError(NUM_TOO_SMALL); }
   if(state[2] != 0){ errMsg = "latches"; return parseError(ILLEGAL_NUM); }
   lineNo++;
   return true;
}

bool
CirMgr::readInput(fstream& file, int& M)
{
   bool missNewLine = false;
   int num;
   string I;
   vector<string> sections;
   getline(file, I);
   if(I == "") {
      if(!file.eof()) { colNo = 0; errMsg = "PI literal ID"; return parseError(MISSING_NUM); }
      errMsg = "PI"; return parseError(MISSING_DEF);
   }
   if(!cutPiece(I, sections, 1, missNewLine)) return false;
   if(myStr2Int(sections[0], num)){
      if(num > (M*2+1)) { colNo = 0; errInt = num; return parseError(MAX_LIT_ID); }
   }else{ errMsg = sections[0]; return parseError(MISSING_NUM); }

   if(num == 0 || num == 1){ colNo = 0; errInt = num; return parseError(REDEF_CONST); }
   if(num%2 == 1){ 
      colNo = 0; errMsg = "PI"; errInt = num;
      return parseError(CANNOT_INVERTED);
   }
   for(int i = 0, s = _piList.size(); i < s; i++){
      if(num/2 == _piList[i]->getGateID()){ 
         errInt = num; errGate = _piList[i]; 
         return parseError(REDEF_GATE); 
      }
   }
   if(missNewLine) return parseError(MISSING_NEWLINE);
   CirGate* PI = new CirPiGate(num, ++lineNo);
   _piList.push_back(PI);
   _totalList[num/2] = PI;
   return true;
}

bool
CirMgr::readOutput(fstream& file, int& M, vector<vector<int>>& fanin)
{
   bool missNewLine = false;
   int num, ID;
   string I;
   vector<string> sections;
   vector<int> out;
   CirGate* PO;
   getline(file, I);
   if(I == "") { 
      if(!file.eof()) { colNo = 0; errMsg = "PO literal ID"; return parseError(MISSING_NUM); }
      errMsg = "PO"; return parseError(MISSING_DEF);
   }
   if(!cutPiece(I, sections, 1, missNewLine)) return false;
   if(myStr2Int(sections[0], num)){
      if(num > (M*2+1)) { colNo = 0; errInt = num; return parseError(MAX_LIT_ID); }
   }else{ errMsg = sections[0]; return parseError(MISSING_NUM); }
   if(_poList.empty()){ ID = (M + 1) * 2; }
   else{ ID = (_poList[_poList.size() - 1]->getGateID() + 1) * 2; }

   for(int i = 0, s = fanin.size(); i < s; i++){
      if(num == fanin[i][1]){ 
         errInt = num; errGate = _totalList[fanin[i][0]]; 
         return parseError(REDEF_GATE); 
      }
   }
   if(missNewLine) return parseError(MISSING_NEWLINE);

   PO = new CirPoGate(ID, ++lineNo);
   _poList.push_back(PO);
   _totalList[ID/2] = PO;

   out.push_back(ID/2); out.push_back(num);
   fanin.push_back(out);
   return true;
}

bool
CirMgr::readAig(fstream& file, int& M, vector<vector<int>>& fanin)
{
   bool missNewLine = false;
   int num, ID2;
   string I;
   vector<string> sections;
   vector<int> gate; 
   getline(file, I);
   if(I == "") { 
      if(!file.eof()) { colNo = 0; errMsg = "AIG literal ID"; return parseError(MISSING_NUM); }
      errMsg = "AIG"; return parseError(MISSING_DEF);
   }
   if(!cutPiece(I, sections, 3, missNewLine)) return false;
   if(sections.size() < 3){ return parseError(MISSING_SPACE); }

   if(myStr2Int(sections[0], num)){
      if(num > (M*2+1)) { colNo = 0; errInt = num; return parseError(MAX_LIT_ID); }
   }else{ errMsg = sections[0]; return parseError(MISSING_NUM); }
   ID2 = num;
   if(ID2 == 0 || ID2 == 1){ colNo = 0; errInt = ID2; return parseError(REDEF_CONST); }
   for(int i = 0, s = _piList.size(); i < s; i++){
      if(ID2/2 == _piList[i]->getGateID()){ 
         errInt = ID2; errGate = _piList[i]; 
         return parseError(REDEF_GATE); 
      }
   }
   for(int i = 0, s = _aigList.size(); i < s; i++){
      if(ID2/2 == _aigList[i]->getGateID()){ 
         errInt = ID2; errGate = _aigList[i]; 
         return parseError(REDEF_GATE); 
      }
   }
   gate.push_back(ID2/2);
   string str_i = to_string(ID2/2);
   colNo = str_i.size();
   for(int i = 1; i < 3; i++){
      colNo += 1;
      if(myStr2Int(sections[i], num)){
         if(num > (M*2+1)) { errInt = num; return parseError(MAX_LIT_ID); }
      }else{ errMsg = sections[i]; return parseError(MISSING_NUM); }
      str_i = to_string(num);
      colNo += (str_i.size());
      gate.push_back(num);
   }
   if(missNewLine) return parseError(MISSING_NEWLINE);

   CirGate* AIG = new AndGate(ID2, ++lineNo);
   _aigList.push_back(AIG);
   _totalList[ID2/2] = AIG;
   fanin.push_back(gate);
   return true;
}

bool
CirMgr::readSymbol(fstream& file, bool& stopRun)
{
   bool missNewLine = false;
   string I;
   vector<string> sections;
   getline(file, I);
   if(I == "") {
      if(file.eof()) cout<<"end"<<endl;
      if(!file.eof()){ colNo = 0; errMsg = ""; return parseError(ILLEGAL_SYMBOL_TYPE); }
      stopRun = true; return true;
   }

   int begin = 0, end = 0, pinID;
   bool prevSpace = false;
   for(int i = 0, s = I.length(); i <= s; i++){
      if(sections.size() == 2){ missNewLine = true; return true; }
      colNo = i;
      if(I[0] == ' ') return parseError(EXTRA_SPACE);
      else if(I[0] == '\t') { errInt = 9; return parseError(ILLEGAL_WSPACE); }
      if(I[i] != ' ' && I[i] != '\t' && I[i] != '\0'){
         if(!isalnum(I[i])){
            errInt = stoi(to_string(I[i]));
            return parseError(ILLEGAL_SYMBOL_NAME);
         }
      }
      if(prevSpace == true){
         if(I[i] == '\t') return parseError(EXTRA_SPACE);
         else{ prevSpace = false; begin = i; }
      }else{
         end = i;
         if(I[i] == ' '){
            sections.push_back(I.substr(begin, end - begin));
            prevSpace = true;
         }else if(I[i] == '\t'){
            if(i != 1) return parseError(MISSING_SPACE);
            else { errInt = 9; return parseError(ILLEGAL_WSPACE); }
         }
         if(end == s){
            string w = I.substr(begin, end - begin + 1);
            if(w != "") sections.push_back(w);
         }
         if(sections.size() == 1){
            if(sections[0].size() == 1){ 
               if(sections[0] == "c"){ 
                  if(I[i] != '\0') return parseError(MISSING_NEWLINE);
                  _commentList.push_back("c");
                  stopRun = true; return true;
               }
               else if(sections[0] != "i" && sections[0] != "o")
                  { colNo = 0; errMsg = sections[0]; return parseError(ILLEGAL_SYMBOL_TYPE); }
               else{ colNo = 1; return parseError(EXTRA_SPACE); }
            }
            else{
               char setIO = sections[0][0];
               string id = sections[0].substr(1, sections[0].size()-1);
               if(setIO != 'i' && setIO != 'o')
                  { colNo = 0; errMsg = sections[0][0]; return parseError(ILLEGAL_SYMBOL_TYPE); }
               if(!myStr2Int(id, pinID))
                  { errMsg = "symbol index("+id+")"; return parseError(ILLEGAL_NUM); }
               if(setIO == 'i'){
                  if(pinID > _piList.size()){ errMsg = "PI index"; errInt = pinID; return parseError(NUM_TOO_BIG); }
                  if(_piList[pinID]->getSymbols() != ""){ errMsg = "i"; errInt = pinID; return parseError(REDEF_SYMBOLIC_NAME); }
               }else if(setIO == 'o'){
                  if(pinID > _poList.size()){ errMsg = "PO index"; errInt = pinID; return parseError(NUM_TOO_BIG); }
                  if(_poList[pinID]->getSymbols() != ""){ errMsg = "o"; errInt = pinID; return parseError(REDEF_SYMBOLIC_NAME); }                  
               }
            }
         }
      }
   }
   if(sections.size() == 1) { errMsg = "symbolic name"; return parseError(MISSING_IDENTIFIER); }
   if(missNewLine) return parseError(MISSING_NEWLINE);
   if(sections[0][0] == 'i')_piList[pinID]->setSymbols(sections[1]);
   else if(sections[0][0] == 'o')_poList[pinID]->setSymbols(sections[1]);

   lineNo++;
   return true;   
}

bool
CirMgr::readComment(fstream& file)
{
   string I;
   vector<string> sections;
   getline(file, I);
   if(I == "") return false;
   _commentList.push_back(I);
   return true;
}

bool
CirMgr::cutPiece(string& pharse, vector<string>& words, int wordNum, bool& missNewLine)
{
   int begin = 0, end = 0;
   bool prevSpace = false;
   for(int i = 0, s = pharse.length(); i <= s; i++){
      if(words.size() == wordNum) {missNewLine = true; return true; }
      colNo = i;
      if(pharse[0] == ' ') return parseError(EXTRA_SPACE);
      else if(pharse[0] == '\t') { errInt = 9; return parseError(ILLEGAL_WSPACE); }
      if(prevSpace == true){
         if(wordNum == 6 && pharse[i] == '\t') { errInt = 9; return parseError(ILLEGAL_WSPACE); }
         if(pharse[i] == ' ' || pharse[i] == '\t') return parseError(EXTRA_SPACE);
         else{ prevSpace = false; begin = i; }
      }else{
         end = i;
         if(pharse[i] == ' '){
            words.push_back(pharse.substr(begin, end - begin));
            prevSpace = true;
         }else if(pharse[i] == '\t'){
            if(wordNum == 2){ errInt = 9; return parseError(ILLEGAL_WSPACE); }
            return parseError(MISSING_SPACE);
         }
         if(end == s){
            string w = pharse.substr(begin, end - begin + 1);
            if(w != "") words.push_back(w);
         }
      }
   }
   return true;
}

void
CirMgr::setFanIO(vector<vector<int>>& fanin)
{
   int ID, inID;
   bool phase;
   for(int i = 0, s = fanin.size(); i < s; i++){
      ID = fanin[i][0];
      for(int j = 1, insize = fanin[0].size(); j < insize; j++){
         inID = fanin[i][j] / 2;
         (fanin[i][j]%2 == 1)? phase = true : phase = false;
         if(_totalList[inID] == 0){
            CirGate* Undef = new UnDef(fanin[i][j]);
            _totalList[inID] = Undef;
         }
         _totalList[ID]->setFanin(_totalList[inID], phase); // set fanin
         _totalList[inID]->setFanout(_totalList[ID]); //set faninout
      }
   }
}

void
CirMgr::dfsTraversal(const GateList& sinkList)
{
   CirGate::setGlobalRef();
   for(int i = 0, s = sinkList.size(); i < s; i++){
      sinkList[i]->dfsTraversal(_dfsList);
   }
}

void
CirMgr::sortFanout()
{
   for(int t = 0, tSize = _totalList.size(); t < tSize; t++){
      if(_totalList[t] != 0) _totalList[t]->sortFanout();
   }
}

/**********************************************************/
/*   class CirMgr member functions for circuit printing   */
/**********************************************************/
/*********************
Circuit Statistics
==================
  PI          20
  PO          12
  AIG        130
------------------
  Total      162
*********************/
void
CirMgr::printSummary() const
{
   cout << endl;
   cout << "Circuit Statistics" << endl;
   cout << "==================" << endl;
   cout << "  " << left << setw(7) << "PI" << right << setw(7) << _piList.size() <<endl;
   cout << "  " << left << setw(7) << "PO" << right << setw(7) << _poList.size() <<endl;
   cout << "  " << left << setw(7) << "AIG" << right << setw(7) << _aigList.size() <<endl;
   cout << "------------------" << endl;
   cout << "  " << left << setw(7) << "Total" << right << setw(7) 
         << _piList.size() + _poList.size() + _aigList.size() <<endl;
}

void
CirMgr::printNetlist() const
{
   vector<Pin> fanin;
   cout << endl;
   for(int i = 0, s = _dfsList.size(); i < s; i++){
      cout << "[" << i << "] ";
      if(typeid(*_dfsList[i]) == typeid(Const0)) cout << "CONST0" << endl;
      else if(typeid(*_dfsList[i]) == typeid(CirPiGate)){
         cout << left << setw(3) << "PI" << " " << _dfsList[i]->getGateID();
         if(_dfsList[i]->getSymbols() != ""){
            cout << " (" << _dfsList[i]->getSymbols() << ")";
         } cout << endl;
      }
      else if(typeid(*_dfsList[i]) == typeid(UnDef)){
         cout << "UNDEF" << " " << _dfsList[i]->getGateID() << endl;
      }
      else{
         if(typeid(*_dfsList[i]) == typeid(CirPoGate))
            cout << left << setw(3) << "PO" << " " << _dfsList[i]->getGateID();
         else if(typeid(*_dfsList[i]) == typeid(AndGate))
            cout << left << setw(3) << "AIG" << " " << _dfsList[i]->getGateID();
         fanin = _dfsList[i]->getFanin();
         for(int j = 0, s = fanin.size(); j < s; j++){
            cout << " ";
            if(typeid(*(fanin[j].getPin())) == typeid(UnDef)) cout << "*";
            if(fanin[j].getInvPhase()) cout << "!";
            cout << fanin[j].getPin()->getGateID();
         } 
         if(_dfsList[i]->getSymbols() != ""){
            cout << " (" << _dfsList[i]->getSymbols() << ")";
         } cout << endl;
      }
   }
}

void
CirMgr::printPIs() const
{
   cout << "PIs of the circuit:";
   for(int i = 0, s = _piList.size(); i < s; i++){
      cout << " " << _piList[i]->getGateID();
   }cout << endl;
}

void
CirMgr::printPOs() const
{
   cout << "POs of the circuit:";
   for(int i = 0, s = _poList.size(); i < s; i++){
      cout << " " << _poList[i]->getGateID();
   }cout << endl;
}

void
CirMgr::printFloatGates() const
{
   vector<Pin> fanin;
   vector<int> floatInGate, NoOutGate;
   CirGate* inPin;
   for(int i = 0, s = _piList.size(); i < s; i++){
      if(_piList[i]->getFanout().size() == 0)
         NoOutGate.push_back(_piList[i]->getGateID());
   }
   for(int i = 0, s = _aigList.size(); i < s; i++){
      fanin = _aigList[i]->getFanin();
      for(int j = 0, inSize = fanin.size(); j < inSize; j++){
         if(typeid(*(fanin[j].getPin())) == typeid(UnDef)){
            floatInGate.push_back(_aigList[i]->getGateID());
            break;
         }
      }
      if(_aigList[i]->getFanout().size() == 0)
         NoOutGate.push_back(_aigList[i]->getGateID());
   }
   for(int i = 0, s = _poList.size(); i < s; i++){
      inPin = _poList[i]->getFanin()[0].getPin();
      if(typeid(*inPin) == typeid(UnDef)) 
         floatInGate.push_back(_poList[i]->getGateID());
   }
   if(floatInGate.size() > 0){
      cout << "Gates with floating fanin(s):";
      for(int i = 0, s = floatInGate.size(); i < s; i++){
         cout << " " << floatInGate[i];
      } cout << endl;
   }
   if(NoOutGate.size() > 0){
      cout << "Gates defined but not used  :";
      for(int i = 0, s = NoOutGate.size(); i < s; i++){
         cout << " " << NoOutGate[i];
      } cout << endl;
   }   
}

void
CirMgr::writeAag(ostream& outfile) const
{
   // header
   int M =  _totalList.size() - _poList.size() - 1, A = 0;
   vector<Pin> fanin;
   for(int i = 0, s = _dfsList.size(); i < s; i++){
      if(typeid(*_dfsList[i]) == typeid(AndGate)) A++;
   }
   outfile << "aag " << M << " " << _piList.size() << " 0 " << _poList.size() << " " <<  A << endl;
   // PI
   for(int i = 0, s = _piList.size(); i < s; i++){
      outfile << myId2Num(_piList[i]->getGateID(), _piList[i]->getInvPhase()) << endl;
   }
   // PO
   for(int i = 0, s = _poList.size(); i < s; i++){
      outfile << myId2Num(_poList[i]->getFanin()[0].getPin()->getGateID(), _poList[i]->getFanin()[0].getInvPhase()) << endl;
   }
   // AIG
   for(int i = 0, s = _dfsList.size(); i < s; i++){
      if(typeid(*_dfsList[i]) == typeid(AndGate)){
         outfile << myId2Num(_dfsList[i]->getGateID(), _dfsList[i]->getInvPhase());
         fanin = _dfsList[i]->getFanin();
         for(int j = 0, faninSize = fanin.size(); j < faninSize; j++){
            outfile << " " << myId2Num(fanin[j].getPin()->getGateID(), fanin[j].getInvPhase());
         } outfile << endl;
      }      
   }
   // Symbols
   for(int i = 0, s = _piList.size(); i < s; i++){
      if(_piList[i]->getSymbols() != "")
         outfile << "i" << _piList[i]->getGateID() << " " << _piList[i]->getSymbols() << endl;
   }
   for(int i = 0, s = _poList.size(); i < s; i++){
      if(_poList[i]->getSymbols() != "")
         outfile << "i" << _poList[i]->getGateID() << " " << _poList[i]->getSymbols() << endl;
   }
   // Comment
   for(int i = 0, s = _commentList.size(); i < s; i++){
      outfile << _commentList[i] << endl;
   }
}

int myId2Num(int id, bool phase)
{
   int Num = id * 2;
   if(phase) Num++;
   return Num;
}