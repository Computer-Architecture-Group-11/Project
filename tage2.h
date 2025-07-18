//
// Created by mohammadmahdi on 7/18/25.
//

#ifndef TAGE2_H
#define TAGE2_H
#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <vector>
#include "../bimodal/bimodal.h"
#include "../gshare/gshare.h"
#include "../hashed_perceptron/hashed_perceptron.h"
#include "../perceptron/perceptron.h"
#include "../../inc/champsim.h"
#include "../../inc/msl/fwcounter.h"
#include "../../inc/msl/lru_table.h"
#include "address.h"
#include "modules.h"
#include "msl/fwcounter.h"



class Entry
{
public:
  champsim::msl::fwcounter<3> predCounter;
  unsigned long long tag;
  champsim::msl::fwcounter<2> uCounter;
  Entry (champsim::msl::fwcounter<3> predCounter, unsigned long long tag, champsim::msl::fwcounter<2> uCounter)
    : predCounter(predCounter), tag(tag), uCounter(uCounter) {}
};

class Table
{
  public:
    int histLength;
    int tagWidth;
    std::vector<Entry> entries;

    Table(int hist_length, int tag_width) : histLength(hist_length), tagWidth(tag_width)
    {
      entries.resize(histLength);
    }
};


class Tage : public champsim::modules::branch_predictor
{
public:
  bimodal biPred;
  int numOfTables = 10;
  int numOfEntries = 2 * 1024;
  double alpha = 1.6;
  int histLenBase = 4;
  int clock = 0;
  std::bitset<5500> GHR;

  std::vector<Table> tables;

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
  void initialize_branch_predictor();

};
#endif //TAGE2_H
