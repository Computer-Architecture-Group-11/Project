
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

#define GHR_SIZE 1024
#define PATH_HISTORY_SIZE 64



class Entry
{
public:
  champsim::msl::fwcounter<3> predCounter;
  unsigned long long tag;
  champsim::msl::fwcounter<2> uCounter;
  Entry(champsim::msl::fwcounter<3> predCounter, unsigned long long tag, champsim::msl::fwcounter<2> uCounter)
      : predCounter(predCounter), tag(tag), uCounter(uCounter) {}
  Entry() : predCounter(0), tag(0), uCounter(0) {}
};

class Table
{
  public:
    int histLength;
    int tagWidth;
    int indexWidth;
    std::vector<Entry> entries;

    Table(int hist_length, int tag_width) : histLength(hist_length), tagWidth(tag_width)
    {
      entries.resize(histLength);
    }
    Table () : histLength(0), tagWidth(0) {}
};


class tage2 : champsim::modules::branch_predictor
{
public:
  bimodal biPred;
  int numOfTables = 10;
  int numOfEntries = 2 * 1024;
  double alpha = 1.6;
  int histLenBase = 8;
  int clock = 0;
  int resetClock = 256 * 1024;
  std::bitset<GHR_SIZE> GHR;
  std::bitset<PATH_HISTORY_SIZE> pathHistory;
  bool providerPrediction;
  bool alterPrediction;
  bool finalPrediction;
  int providerIdx = -1;
  int alterIdx = -1;
  bool uResetType1 = true; //type 1 is with most significant bit and if not, the least significant
  champsim::msl::fwcounter<4> altBetterThanProvider;

  std::vector<Table> tables;

  using branch_predictor::branch_predictor;
  tage2(O3_CPU* cpu): branch_predictor(cpu), biPred(cpu) {}

  bool predict_branch(champsim::address ip);
  void last_branch_result(champsim::address ip, champsim::address branch_target, bool taken, uint8_t branch_type);
  void initialize_branch_predictor();
  void reset_u();
};
#endif //TAGE2_H
