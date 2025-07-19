//
// Created by mohammadmahdi on 7/18/25.
//

#include "tage2.h"



void Tage::initialize_branch_predictor()
{
  tables.resize(numOfTables);
  for (int i = 0; i < numOfTables; i++) {
    tables[i].tagWidth = 8;           // to change later
    tables[i].histLength = static_cast<int>(histLenBase * pow(alpha, i - 1) + 0.5);
    for (int j = 1; j < numOfEntries; j++) {
      tables[i].entries[j].predCounter = champsim::msl::fwcounter<3>(4);
      tables[i].entries[j].uCounter = champsim::msl::fwcounter<2>(0);
    }
  }
}

unsigned long long foldHist(std::bitset<5500> GHR, int histLength, int compLength)
{
  unsigned long long folded = 0;
  for (int i = 0; i < histLength; ++i) {
    folded ^= (GHR[i] << (i % compLength));
  }
  return folded & ((1ull << compLength) - 1);
}

unsigned long long hash_index(champsim::address ip, std::bitset<5500> GHR, const Table& table, int numOfEntries)
{
  int compLength = static_cast<int>(log2(table.histLength));
  auto pc = ip.to<unsigned long long>();
  unsigned long long folded_history = foldHist(GHR, table.histLength, compLength);

  unsigned long long index = pc ^ (pc >> 2) ^ folded_history;

  return index % numOfEntries; // compLength = log2(table_size)
}

unsigned long long hash_tag(champsim::address ip, std::bitset<5500> GHR, const Table& table)
{
  auto pc = ip.to<unsigned long long>();
  unsigned long long folded_history = foldHist(GHR, table.histLength, table.tagWidth);

  unsigned long long tag = pc ^ (pc >> 3) ^ (folded_history << 1);

  return tag & ((1 << table.tagWidth) - 1); 
}
bool Tage::predict_branch(champsim::address ip)
{
  providerIdx = -1;
  alterIdx = -1;
  int index_pred;
  int alter_pred;
  for (int i = numOfTables - 1; i >= 0; i--) {
    unsigned long long tag = hash_tag(ip, GHR, tables[i]);
    unsigned long long idx = hash_index(ip, GHR, tables[i], numOfEntries);

    if (tables[i].entries[idx].tag == tag) {
      if (providerIdx == -1) {
        providerIdx = i;
        index_pred = idx;
      } else if (alterIdx == -1) {
        alterIdx = i;
        alter_pred = idx;
        break;
      }
    }
  }
  alterPrediction = biPred.predict_branch(ip);
  if (providerIdx != -1) {
    if(alterIdx != -1) {
      alterPrediction = tables[alterIdx].entries[alter_pred].predCounter >= 4;
    }
    if ((tables[providerIdx].entries[index_pred].predCounter != 3 &&
        tables[providerIdx].entries[index_pred].predCounter != 4 ) ||
        tables[providerIdx].entries[index_pred].uCounter.value() != 0) {   
               //TODO if alter was many times better
      providerPrediction = tables[providerIdx].entries[index_pred].predCounter >= 4;
      return providerPrediction;
    } 
  } 
  return alterPrediction; //if the base predictor is used we will consider it as an alter prediction
}

void Tage::reset_u() {
  for(Table table : tables) {
    for (Entry entry : table.entries) {
      if(uResetType1){
        entry.uCounter = champsim::msl::fwcounter<2>(entry.uCounter.value() & 0b01);
      }else{
        entry.uCounter = champsim::msl::fwcounter<2>(entry.uCounter.value() & 0b10);
      }
    }
  }
  uResetType1 = !uResetType1;
}

void Tage::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  biPred.last_branch_result(ip, branch_target, taken, branch_type);
  bool prediction = alterPrediction;
  if(providerIdx != -1) {
    unsigned long long idx = hash_index(ip, GHR, tables[providerIdx], numOfEntries);
    if(taken) {
      tables[providerIdx].entries[idx].predCounter++;
    } else {
      tables[providerIdx].entries[idx].predCounter--;
    }

    if(providerPrediction != alterPrediction){
      if(providerPrediction == taken) {
        tables[providerIdx].entries[idx].uCounter++;
      } else {
        tables[providerIdx].entries[idx].uCounter--;
      }
    }
  }

 
  GHR <<= 1;
  GHR[0] = taken;
  clock++;
  if(clock >= resetClock){
    clock = 0;
    reset_u();
  }
  
}



