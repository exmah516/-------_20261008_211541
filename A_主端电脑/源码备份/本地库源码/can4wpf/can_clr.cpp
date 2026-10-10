// compile with: /clr
using namespace System;
using namespace System::IO;

#include "copleycan.h"

namespace Ercp {
    namespace Module {
        public ref class CCan {
        private:
            CopleyCan* mPcc = NULL;
            int s = 0;

        public :
            CCan() {
                mPcc = new CopleyCan();
            }
            ~CCan() {
                delete mPcc;
            }

            int Open() {
                return mPcc->Open(0, COPLEYCAN_BITRATE_500000);                
            }

            bool IsOpen() { return mPcc->IsOpen(); }

            int Recv([Runtime::InteropServices::Out]uint8_t % type,
                [Runtime::InteropServices::Out]uint8_t % length,
                [Runtime::InteropServices::Out]UInt32 % id,
                [Runtime::InteropServices::Out]UInt32 % timestamp,
                [Runtime::InteropServices::Out]array<uint8_t> ^% data) {
                CanFrame f;
                int iErr = mPcc->Recv(&f, 20);
                type = f.type;
                length = f.length;
                id = f.id;
                timestamp = f.timestamp;
                s = timestamp;
                data = gcnew array<uint8_t>(8);
                for (int i = 0; i < 8; i++) {
                    data[i] = f.data[i];
                }
                return iErr;
            }

            void SetUseMaster(bool bIsMaster) {
                CanFrame f;
                //int iErr = mPcc->Recv(&f, 5000);
                f.type = 0;
                f.length= 8;
                f.id = 0x00000012;
                f.data[0] = 0x00000061;
                f.data[6] = bIsMaster ? 0x00000080 : 0;

                mPcc->Xmit(&f, 10);

            }
        };
    }
}