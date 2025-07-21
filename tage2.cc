
#include "tage2.h"
#include <random>

void tage2::initialize_branch_predictor()
{
  altBetterThanProvider = champsim::msl::fwcounter<4>(0);
  tables.resize(numOfTables);
  double tagWidth = 12;
  for (int i = 0; i < numOfTables; i++) {
    tagWidth -= 0.33;
    tables[i].tagWidth = static_cast<int>(tagWidth);     
    tables[i].indexWidth = 10;     
    tables[i].histLength = static_cast<int>(histLenBase * pow(alpha, i - 1) + 0.5);
    tables[i].entries.resize((1 << tables[i].indexWidth) + 10);
    for (int j = 0; j < (1 << tables[i].indexWidth); j++) {
      tables[i].entries[j].predCounter = champsim::msl::fwcounter<3>(4);
      tables[i].entries[j].uCounter = champsim::msl::fwcounter<2>(0);
    }
  }
}

unsigned long long foldHist(const std::bitset<GHR_SIZE> &GHR, int histLength, int compLength)
{
  unsigned long long temphist = 0;
  unsigned long long comphist = 0;
    for (int i = 0; i < histLength; i++)
    {
        if (i % compLength == 0)
        {
            comphist ^= temphist;
            temphist = 0;
        }
        temphist = (temphist << 1) | GHR[i];
    }
    comphist ^= temphist;
    return comphist;
}
unsigned long long path_history_hash(const std::bitset<PATH_HISTORY_SIZE> &pathHistory, int tableIndex, const std::vector<Table>& tables)
{
    unsigned long long A = 0;
    Table table = tables[tableIndex];
    
    int size = table.histLength > 16 ? 16 : table.histLength; 
    for (int i = PATH_HISTORY_SIZE - 1; i >= 0; i--)
    {
        A = (A << 1) | pathHistory[i]; 
    }

    A = A & ((1 << size) - 1);
    unsigned long long A1;
    unsigned long long A2;
    A1 = A & ((1 << table.indexWidth) - 1); 
    A2 = A >> table.indexWidth; 
    A2 = ((A2 << tableIndex) & ((1 << table.indexWidth) - 1)) + (A2 >> abs(table.indexWidth - tableIndex));
    A = A1 ^ A2;
    A = ((A << tableIndex) & ((1 << table.indexWidth) - 1)) + (A >> abs(table.indexWidth - tableIndex));
    
    return (A);
}

unsigned long long hash_index(champsim::address ip, const std::bitset<GHR_SIZE> &GHR, const std::bitset<PATH_HISTORY_SIZE> &pathHistory, int tableIndex, const std::vector<Table> &tables)
{
  Table table = tables[tableIndex];
  unsigned long long path_history = path_history_hash(pathHistory, tableIndex, tables); 

  unsigned long long global_history_hash = foldHist(GHR, table.histLength, table.indexWidth);
  unsigned long long ip2 = ip.to<unsigned long long>();

  return (global_history_hash ^ ip2 ^ (ip2 >> (abs(table.indexWidth - tableIndex) + 1)) ^ path_history) & ((1 << table.indexWidth) - 1);
}

unsigned long long hash_tag(champsim::address ip, const std::bitset<GHR_SIZE> GHR, int tableIndex, const std::vector<Table> &tables)
{ 
  Table table = tables[tableIndex];
  unsigned long long global_history_hash = foldHist(GHR, table.histLength, table.tagWidth);
  global_history_hash ^= foldHist(GHR, table.histLength, table.tagWidth - 1);
    
  return (global_history_hash ^ ip.to<unsigned long long>()) & ((1 << table.tagWidth) - 1);
}

bool tage2::predict_branch(champsim::address ip)
{
  providerIdx = -1;
  alterIdx = -1;
  int index_pred;
  int alter_pred;
  for (int i = numOfTables - 1; i >= 0; i--) {
    unsigned long long tag = hash_tag(ip, GHR, i, tables);
    unsigned long long idx = hash_index(ip, GHR, pathHistory, i, tables);

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
  finalPrediction = alterPrediction;
  if (providerIdx != -1) {
    if(alterIdx != -1) {
      alterPrediction = tables[alterIdx].entries[alter_pred].predCounter >= 4;
    }
    providerPrediction = tables[providerIdx].entries[index_pred].predCounter >= 4;
    if ((tables[providerIdx].entries[index_pred].predCounter != 3 &&
      tables[providerIdx].entries[index_pred].predCounter != 4 ) ||
      tables[providerIdx].entries[index_pred].uCounter.value() != 0 ||
      altBetterThanProvider <= 7) {   
      finalPrediction = providerPrediction;
    }else{
      finalPrediction = alterPrediction;
    }
  } 
  return finalPrediction; //if the base predictor is used we will consider it as an alter prediction
}

void tage2::reset_u() {
  for(Table& table : tables) {
    for (Entry& entry : table.entries) {
      if(uResetType1){
        entry.uCounter = champsim::msl::fwcounter<2>(entry.uCounter.value() & 0b01);
      }else{
        entry.uCounter = champsim::msl::fwcounter<2>(entry.uCounter.value() & 0b10);
      }
    }
  }
  uResetType1 = !uResetType1;
}

void tage2::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{
  biPred.last_branch_result(ip, branch_target, taken, branch_type);
  if(providerIdx != -1) {
    unsigned long long idx = hash_index(ip, GHR, pathHistory, providerIdx, tables);
    int counter = tables[providerIdx].entries[idx].predCounter.value();
    if(taken && counter < 7) {
      counter++;
    } else if (!taken && counter > 0) {
      counter--;
    }
    tables[providerIdx].entries[idx].predCounter = champsim::msl::fwcounter<3>(counter);
    if(providerPrediction != alterPrediction){
      if(providerPrediction == taken) {
        tables[providerIdx].entries[idx].uCounter++;
        altBetterThanProvider--;
      } else {
        tables[providerIdx].entries[idx].uCounter--;
        altBetterThanProvider++;
      }
    }

  }
  if(finalPrediction != taken) {
    long rand_val = rand();
    int start_table = providerIdx + 1 + (rand_val & 1) + ((rand_val >> 1) & 1);
    
    bool found = false;
    for (int i = start_table; i < numOfTables; i++) {
      unsigned long long idx = hash_index(ip, GHR, pathHistory, i, tables);
      if (tables[i].entries[idx].uCounter.value() == 0) {
        tables[i].entries[idx] = Entry(champsim::msl::fwcounter<3>(4),
                                       hash_tag(ip, GHR, i, tables),
                                       champsim::msl::fwcounter<2>(0));
        found = true;
        break;
      }
    }
    if (!found && start_table < numOfTables) {
      unsigned long long idx = hash_index(ip, GHR, pathHistory, start_table, tables);
      tables[start_table].entries[idx].uCounter = champsim::msl::fwcounter<2>(0);
    }
    
  }

  GHR <<= 1;
  GHR[0] = taken;
  pathHistory <<= 1;
  pathHistory[0] = ip.to<int>() & 1;
  clock++;
  if(clock >= resetClock){
    clock = 0;
    reset_u();
  }
  
}



