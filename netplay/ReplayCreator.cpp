#include "ReplayCreator.hpp"
#include "StringUtils.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <sstream>
#include <map>
using namespace std;

void ReplayCreator::dump(ReplayCreator::ReplayFile rf, char* fname) {
    ofstream outfile;
    outfile.open(fname, ios::binary | ios::out);
    //cout << "dumping" << endl;
    outfile.write((char*)&rf, 0x60);
    //cout << rf.numRounds << endl;
    for (int j=0; j < rf.numRounds; ++j) {
      //cout << j << endl;
      ReplayCreator::Round round = rf.rounds[j];
      outfile.write((char*)&round, 0x8C);
      outfile.write((char*)&round.lenp1Inputs, 4);
      for (int i = 0; i < round.lenp1Inputs; ++i) {
        ReplayCreator::Input in = round.p1Inputs[i];
        outfile.write((char*)&round.p1Inputs[i], 6);
      }
      outfile.write((char*)&round.lenp2Inputs, 4);
      for (int i = 0; i < round.lenp2Inputs; ++i) {
        outfile.write((char*)&round.p2Inputs[i], 6);
      }
      outfile.write((char*)&round.lenp3Inputs, 4);
      for (int i = 0; i < round.lenp3Inputs; ++i) {
          outfile.write((char*)&round.p3Inputs[i], 6);
      }
      outfile.write((char*)&round.lenp4Inputs, 4);
      for (int i = 0; i < round.lenp4Inputs; ++i) {
          outfile.write((char*)&round.p4Inputs[i], 6);
      }
      outfile.write((char*)&round.lenRng, 4);
      for (int i = 0; i < round.lenRng; ++i) {
        outfile.write((char*)&round.rngstates[i], 4);
      }
      outfile.write((char*)&round.nine, 4*36);
    }
    outfile.close();
}

void ReplayCreator::load(ReplayCreator::ReplayFile* rf, char* fname) {
    ifstream infile;
    infile.open(fname, ios::binary | ios::in);
    infile.read((char*)rf, 0x60);
    //cout << formatAsHex((void*) rf, 0x60) << endl;
    //cout << endl;
    for (int j=0; j < rf->numRounds; ++j) {
      ReplayCreator::Round roundObj;
      ReplayCreator::Round* round = &roundObj;
      infile.read((char*)round, 0x8C);
      //cout << formatAsHex((void*) round, 0x8c) << endl;
      infile.read((char*)&(round->lenp1Inputs), 4);
      //printf("Player1\n");
      //printf("%d %d\n", round->lenp1Inputs, round->lenp2Inputs);
      ReplayCreator::Input in;
      for (int i = 0; i < round->lenp1Inputs; ++i) {
        infile.read((char*)&in, 6);
        round->p1Inputs.push_back(in);
      }
      infile.read((char*)&(round->lenp2Inputs), 4);
      //printf("Player2\n");
      //printf("%d %d\n", round->lenp1Inputs, round->lenp2Inputs);
      for (int i = 0; i < round->lenp2Inputs; ++i) {
        infile.read((char*)&in, 6);
        //cout << formatAsHex((void*) &in, 6) << endl;
        //cout << "preprint" << endl;
        //printInput2(in);
        //cout << "postprint" << endl;
        round->p2Inputs.push_back(in);
      }
      // A copy of p1/p2's inputs, with some directions changed. May or not appear.
      // Don't know why this exists either, doesn't seem to do anything
      // Probably something to do with controllers
      infile.read((char*)&(round->lenp3Inputs), 4);
      //cout << formatAsHex((void*) &(round->lenp3Inputs), 4) << endl;
      //printf("Player3\n");
      //printf("%d\n", round->lenp3Inputs);
      for (int i = 0; i < round->lenp3Inputs; ++i) {
          infile.read((char*)&in, 6);
          //cout << formatAsHex((void*) &in, 6) << endl;
          //cout << "preprint" << endl;
          //printInput2(in);
          //cout << "postprint" << endl;
          round->p3Inputs.push_back(in);
      }
      infile.read((char*)&(round->lenp4Inputs), 4);
      //printf("Player4\n");
      //printf("%d\n", round->lenp4Inputs);
      for (int i = 0; i < round->lenp4Inputs; ++i) {
          infile.read((char*)&in, 6);
          //cout << formatAsHex((void*) &in, 6) << endl;
          //cout << "preprint" << endl;
          //printInput2(in);
          //cout << "postprint" << endl;
          round->p4Inputs.push_back(in);
      }

      infile.read((char*)&(round->lenRng), 4);
      int tmp;
      for (int i = 0; i < round->lenRng; ++i) {
        infile.read((char*)&tmp, 4);
        //cout << formatAsHex((void*) &tmp, 4) << endl;
        round->rngstates.push_back(tmp);
      }
      infile.read((char*)&(round->nine), 144);
      //cout << formatAsHex((void*) round->nine, 144) << endl;
      //cout << "aa" << endl;
      rf->rounds.push_back(roundObj);
    }
}
void ReplayCreator::fixReplay(ReplayCreator::ReplayFile* rf, char* fname, MoveData* prior) {
    if (!rf || !fname) {
        LOG("ReplayCreator::fixReplay: invalid arguments (rf=%p fname=%p)", rf, fname);
        return;
    }

    LOG("asdf");

    ifstream infile;
    infile.open(fname);
    if (!infile.is_open()) {
        LOG("ReplayCreator::fixReplay: failed to open input '%s'", fname);
        return;
    }

    string line;
    if (!getline(infile, line)) {
        LOG("ReplayCreator::fixReplay: missing round count header");
        return;
    }

    int rounds = 0;
    try {
        rounds = stoi(line);
    } catch ( const std::exception& ex ) {
        LOG("ReplayCreator::fixReplay: invalid round header '%s' (%s)", line.c_str(), ex.what());
        return;
    }

    LOG("line: %s", line.c_str());
    LOG("rounds: %d", rounds);

    if (rounds <= 0) {
        LOG("ReplayCreator::fixReplay: no rounds to process (%d)", rounds);
        return;
    }

    if (rf->rounds.size() < static_cast<size_t>(rounds)) {
        LOG("ReplayCreator::fixReplay: replay metadata has %zu rounds but raw file lists %d; clamping",
            rf->rounds.size(), rounds);
        rounds = static_cast<int>(rf->rounds.size());
    }

    for (int i = 0; i < rounds; ++i) {
        if (i >= static_cast<int>(rf->rounds.size())) {
            LOG("ReplayCreator::fixReplay: ran out of replay rounds at index %d", i);
            break;
        }

        rf->rounds[i].p1Inputs.clear();
        rf->rounds[i].p2Inputs.clear();

        if (!getline(infile, line)) {
            LOG("ReplayCreator::fixReplay: missing frame count for round %d", i);
            break;
        }

        int frames = 0;
        try {
            frames = stoi(line);
        } catch ( const std::exception& ex ) {
            LOG("ReplayCreator::fixReplay: invalid frame count '%s' (%s) for round %d",
                line.c_str(), ex.what(), i);
            break;
        }

        LOG("line: %s", line.c_str());
        LOG("round number: %d", i);
        LOG("frames: %d", frames);

        MoveData p1data = {};
        MoveData p2data = {};

        /*
        int duration = 0;
        int numInputs = 0;
        uint8_t button = 0;
        uint8_t lastButton = 0;
        uint8_t direction = 0;
        uint8_t lastDirection = 0;
        uint8_t down = 0;
        uint8_t lastDown = 0;
        uint8_t up = 0;
        uint8_t lastUp = 0;
        bool last = true;

        int duration2 = 0;
        int numInputs2 = 0;
        uint8_t button2 = 0;
        uint8_t lastButton2 = 0;
        uint8_t direction2 = 0;
        uint8_t lastDirection2 = 0;
        uint8_t down2 = 0;
        uint8_t lastDown2 = 0;
        uint8_t up2 = 0;
        uint8_t lastUp2 = 0;
        bool last2 = true;
        */

        vector<string> frameInputs1;
        vector<string> frameInputs2;
        bool incomplete = false;
        for (int j = 0; j < frames; ++j) {
            string p1Input;
            string p2Input;
            if (!getline(infile, p1Input, ' ')) {
                LOG("ReplayCreator::fixReplay: truncated p1 input at frame %d (round %d)", j, i);
                incomplete = true;
                break;
            }
            if (!getline(infile, p2Input)) {
                LOG("ReplayCreator::fixReplay: truncated p2 input at frame %d (round %d)", j, i);
                incomplete = true;
                break;
            }
            frameInputs1.push_back( p1Input );
            frameInputs2.push_back( p2Input );

        }
        if (incomplete) {
            break;
        }
        //cout << "endframeread" << endl;
        //cout << "p1 " <<  endl;
        LOG( "writeInputs" );
        writeInputs( rf->rounds[i].p1Inputs,
                     frameInputs1,
                     NULL );
                     //&prior[0] );
        //cout << "p2 " << endl;
        //frameInputs2.erase(frameInputs2.begin(), frameInputs2.begin()+3);
        writeInputs( rf->rounds[i].p2Inputs,
                     frameInputs2, NULL ); //&prior[1] );
        rf->rounds[i].lenp1Inputs = rf->rounds[i].p1Inputs.size();
        rf->rounds[i].lenp2Inputs = rf->rounds[i].p2Inputs.size();

    }

    for (Input in : rf->rounds[0].p1Inputs ) {
      //printInput2(in);
    }

    //cout << "endfix" << endl;

}

uint8_t ReplayCreator::getDirection(unsigned int x) {
    uint8_t direction = x & 0xF;
    if (x & 0x0020) {
        if (direction == 1) {
            direction = 3;
        } else if (direction == 3) {
            direction = 1;
        }
        if (direction == 4) {
            direction = 6;
        } else if (direction == 6) {
            direction = 4;
        }
        if (direction == 7) {
            direction = 9;
        } else if (direction == 9) {
            direction = 7;
        }
    }
    return direction;
}

uint8_t ReplayCreator::getButton(unsigned int x) {
    uint8_t button = 0;
    if ((x & 0x4100) != 0) {
        button |= 1;
    }
    if ((x & 0x8200) != 0) {
        button |= 2;
    }
    if ((x & 0x0400) != 0) {
        button |= 3;
    }
    if ((x & 0x0080) != 0) {
        button |= 4;
    }
    if ((x & 0x0040) != 0) {
        button |= 8;
    }
    if ((x & 0x0800) != 0) {
        button |= 16;
    }
    return button;
}


string ReplayCreator::getDirIcon(uint8_t x) {
    switch(x) {
    case 1:
        return "\u2199";
    case 2:
        return "\u2b07";
    case 3:
        return "\u2198";
    case 4:
        return "\u2b05";
    case 6:
        return "\u27a1";
    case 7:
        return "\u2196";
    case 8:
        return "\u2b06";
    case 9:
        return "\u2197";
    }

    return " ";
}


string ReplayCreator::getButtonIcon(uint8_t x) {
    string ret = "";
    if ( x & 1 ) {
        ret.append("A");
    }
    if ( x & 2 ) {
        ret.append("B");
    }
    if ( x & 4 ) {
        ret.append("C");
    }
    if ( x & 8 ) {
        ret.append("D");
    }
    if ( x & 16 ) {
        ret.append("E");
    }
    return ret;
}

void ReplayCreator::printinfo( ReplayCreator::ReplayFile* rf ) {
  cout <<  "numwins " << rf->numWins << endl;
  cout <<  "numrounds" << rf->numRounds << endl;
  cout << rf->rounds[0].p1Inputs.size() << endl;
  cout << rf->rounds[1].p1Inputs.size() << endl;
  /*
  for (int i; i < 10; ++i) {
    cout << "p1" << endl;
    printInput2(rf->rounds[0].p1Inputs[i]);
    cout << "p2" << endl;
    printInput2(rf->rounds[0].p2Inputs[i]);
  }
  */
}


void ReplayCreator::printrepdiff( ReplayCreator::ReplayFile* rf1,
                                  ReplayCreator::ReplayFile* rf2,
                                  int p) {
  cout << "printing" << endl;
  for (int i = 0; i < rf1->rounds[0].lenp1Inputs; ++i) {
    cout << i << endl;
    if ( p== 1) {
      cout << "p1" << endl;
      printInput2(rf1->rounds[0].p1Inputs[i]);
      printInput2(rf2->rounds[0].p1Inputs[i]);
    } else {
      cout << "p2" << endl;
      printInput2(rf1->rounds[0].p2Inputs[i]);
      printInput2(rf2->rounds[0].p2Inputs[i]);
    }
  }
}

void ReplayCreator::findrepdiff( ReplayCreator::ReplayFile* rf1,
                                 ReplayCreator::ReplayFile* rf2,
                                 int tol,
                                 int round
                                 ) {
  cout << "finding" << endl;
  cout << "p1 max " << rf1->rounds[round].lenp1Inputs << endl;
  int tol2 = tol;
  int total = 0;
  int back = 8;
  int forward = 8;
  for (int i = 0; i < rf1->rounds[round].lenp1Inputs; ++i) {
      total += rf1->rounds[round].p1Inputs[i].duration;
    if ( rf1->rounds[round].p1Inputs[i] != rf2->rounds[round].p1Inputs[i]) {
        int tb = back;
        while ( tb > 0 ) {
            if ( i >= tb ) {
                cout << i-tb << endl;
                printInput2(rf1->rounds[round].p1Inputs[i-tb]);
                printInput2(rf2->rounds[round].p1Inputs[i-tb]);
            }
            tb--;
        }
        cout << i << " <--- @ " << total << endl;
        printInput2(rf1->rounds[round].p1Inputs[i]);
        printInput2(rf2->rounds[round].p1Inputs[i]);
        int tf = 1;
        while ( tf <= forward ) {
            cout << i+tf << endl;
            printInput2(rf1->rounds[round].p1Inputs[i+tf]);
            printInput2(rf2->rounds[round].p1Inputs[i+tf]);
            tf++;
        }
        cout << endl;
        tol--;
        if (tol == 0)
            break;
    }
  }
  cout << "p2 max " << rf1->rounds[round].lenp2Inputs << endl;
  for (int i = 0; i < rf1->rounds[round].lenp2Inputs; ++i) {
    if ( rf1->rounds[round].p2Inputs[i] != rf2->rounds[round].p2Inputs[i]) {
      cout << i-1 << endl;
      printInput2(rf1->rounds[round].p2Inputs[i-1]);
      printInput2(rf2->rounds[round].p2Inputs[i-1]);
      cout << i << " <---" << endl;
      printInput2(rf1->rounds[round].p2Inputs[i]);
      printInput2(rf2->rounds[round].p2Inputs[i]);
      cout << i+1 << endl;
      printInput2(rf1->rounds[round].p2Inputs[i+1]);
      printInput2(rf2->rounds[round].p2Inputs[i+1]);
      tol2--;
      if (tol2 == 0)
          break;
    }
  }
}

void ReplayCreator::findrngdiff( ReplayCreator::ReplayFile* rf1,
                                 ReplayCreator::ReplayFile* rf2
                                 ) {
  cout << "finding" << endl;
  cout << "p1" << endl;
  int tol = 10;
  for (int i = 0; i < rf1->rounds[0].rngstates.size(); ++i) {
    if ( rf1->rounds[0].rngstates[i] != rf2->rounds[0].rngstates[i]) {
      cout << i << " <---" << endl;
      cout << formatAsHex((void*)&(rf1->rounds[0].rngstates[i]), 4) << " ";
      cout << formatAsHex((void*)&(rf2->rounds[0].rngstates[i]), 4) << endl;
      tol--;
      if (tol == 0)
          break;
    }
  }
}

void ReplayCreator::printInput3( ReplayCreator::Input input ) {
    string t = getButtonIcon( input.buttonHold );
    int l = strlen(t.c_str());
    cout << getDirIcon( input.direction ) << " " <<
        t << " " <<
        setw(10-l) <<
        unsigned( input.duration ) << //" " <<
        //unsigned( input.buttonDown ) << " " <<
        //unsigned( input.buttonUp ) << " " <<
        //unsigned( input.button4 ) <<
        endl;
}

void ReplayCreator::printInput2( ReplayCreator::Input input ) {
  cout << unsigned( input.duration ) << " " <<
    unsigned( input.direction ) << " " <<
    unsigned( input.buttonHold ) << " " <<
    unsigned( input.buttonDown ) << " " <<
    unsigned( input.buttonUp ) << " " <<
    unsigned( input.button4 ) << endl;
}

void ReplayCreator::printInput( ReplayCreator::Input input ) {
  cout << "printintput" << endl;
  cout <<  "duration " << unsigned( input.duration ) << endl;
  cout <<  "direction " << unsigned( input.direction ) << endl;
  cout <<  "buttonHold " << unsigned( input.buttonHold ) << endl;
  cout <<  "buttonDown " << unsigned( input.buttonDown ) << endl;
  cout <<  "buttonUp " << unsigned( input.buttonUp ) << endl;
  //cout <<  "button4 " << unsigned( input.button4 ) << endl;
}

void ReplayCreator::readFrameInput( ReplayCreator::MoveData* m,
                                    string frameInput ) {
    unsigned int rawinput = std::stoul(frameInput, nullptr, 16);
    m->direction = getDirection(rawinput);
    m->button = getButton(rawinput);
    m->down = BUTTON_DOWN(m->lastButton, m->button);
    m->up = BUTTON_UP(m->lastButton, m->button);
}

void ReplayCreator::copyToInput( ReplayCreator::MoveData* m,
                                 ReplayCreator::Input &input ) {
  input.direction = m->lastDirection;
  input.duration = m->duration;
  input.buttonHold = m->lastButton;
  input.buttonDown = m->lastDown;
  input.buttonUp = m->lastUp;
}


void ReplayCreator::writeInputs( vector<ReplayCreator::Input> &inputs,
                                 vector<string> &rawinputs, MoveData* prior ) {
    MoveData data = {};
    bool first = true;
    bool printing = false;
    bool last;
    Input input;
    input.button4 = 0;
    //cout << "writeInputs " << endl;
    int loc = 0;
    for ( string s : rawinputs ) {
        if (printing) {
          cout << "rawstring " << s << endl;
          cout << data.duration << endl;
        }
        last = true;
        readFrameInput( &data, s );
        if ( ( prior != NULL ) && first ) {
            cout << "reading prior data " << s << endl;
            first = false;
            data.duration = 1;
            data.lastDirection = prior->direction;
            data.lastButton = prior->button;
            data.lastDown = prior->down;
            data.lastUp = prior->up;
        } else if (first) {
            first = false;
            data.lastDirection = data.direction;
            data.lastButton = data.button;
            data.lastDown = data.down;
            data.lastUp = data.up;
        }

        if ( data.direction != data.lastDirection ||
             data.button != data.lastButton ||
             data.down != data.lastDown ||
             data.up != data.lastUp
             ) {
            //int x = 0;
                if ((data.lastButton == 16) &&
                    (data.button == data.lastButton ) &&
                    (data.lastUp == 7) &&
                    (data.direction == data.lastDirection)) {
                    data.duration += 1;
                    loc++;
                    continue;
                }
                copyToInput( &data, input);
                LOG( "rawframe %d input ", loc );
                if ( printing ) {
                    cout << "rawframe " << loc << " input " << inputs.size()<< endl;
                  printInput2(input);
                }
                data.duration = 0;
                if (data.lastDown & 16) {
                    //Macro
                    switch ( data.lastDirection )
                    {
                        case 0:
                            data.up |= 7;
                            break;
                        case 2:
                            data.up |= 3;
                            break;
                        case 4:
                        case 6:
                            data.up |= 9;
                            break;
                        default:
                            break;
                    }
                }
                if ((data.lastButton & 16) && (data.lastUp == 7)) {
                    data.up |= 7;
                }
                data.lastDirection = data.direction;
                data.lastButton = data.button;
                data.lastDown = data.down;
                data.lastUp = data.up;
                inputs.push_back(input);
                last = false;
        } else if ( data.duration == 248 ) {
            copyToInput( &data, input);
            data.duration = 0;
            //cout << "printdata" << endl;
            //printInput2(input);
            inputs.push_back(input);
            last = false;
        }

        data.duration += 1;
        loc++;
    }

    if (last) {
        copyToInput( &data, input);
        //printInput(input);
        inputs.push_back(input);
    }
    //cout << "outfix" << endl;
}


void ReplayCreator::fixRng( ReplayFile* rf, char* fname ) {
    // Uses the RNG seed to determine if the next state has been seen before(past 10) or is a valid transition(next 10)
    // This is a really poor way of doing it, will need to find a better way later, which will probably require hooking immediately before the write
    // To Implement: actually checking the future states
    //Actually this probably isn't even necessary
    ifstream infile;
    infile.open(fname);
    string line;
    getline(infile,line);
    //cout << line << endl;
    //int rounds = stoi(line);
    uint32_t seenRng[10];
    int position = 0;
    //for (int i = 0; i < rf->numRounds; ++i) {
    for (int i = 0; i < rf->numRounds; ++i) {
        //getline(infile, line);
        //RngState rng = getRng(line);
        Round r = rf->rounds[i];
    }
}

void ReplayCreator::printReplay( ReplayFile* rf, int start, int end, int round) {
  int total = 0;
  int e1 = rf->rounds[round].lenp1Inputs;
  if ( end != 0 && e1 > end )
      e1 = end;
  int e2 = rf->rounds[round].lenp2Inputs;
  if ( end != 0 && e2 > end )
      e2 = end;
  int dur = 0;
  vector<Input> p1 = rf->rounds[round].p1Inputs;
  vector<Input> p2 = rf->rounds[round].p2Inputs;
  cout << "p1 max " << rf->rounds[round].lenp1Inputs << endl;
  for (int i = start; i < e1; ++i) {
    total += p1[i].duration;
    cout << i << " <--- @ " << total << endl;
    if ( i < e1-1 &&
         p1[i].direction == p1[i+1].direction &&
         p1[i].buttonHold == p1[i+1].buttonHold ) {
        p1[i+1].duration += p1[i].duration;
    } else {
        printInput3(p1[i]);
    }
  }
  cout << "p2 max " << rf->rounds[round].lenp2Inputs << endl;
  for (int i = start; i < e2; ++i) {
      //cout << i << " <---" << endl;
      printInput3(p2[i]);
  }
}
