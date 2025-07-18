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

bool Tage::predict_branch(champsim::address ip)
{

}

void Tage::last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type)
{

}



