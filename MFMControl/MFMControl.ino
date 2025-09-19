// MFM-Control input pins (active low):
// A0(PC0) ... -SEEK COMPLETE
// A1(PC1) ... -TRACK 0
// A2(PC2) ... -READY
// MFM-Control output pins (active low):
// D8(PB0)  ... -HEAD SELECT 0
// D9(PB1)  ... -HEAD SELECT 1
// D10(PB2) ... -HEAD SELECT 2
// D11(PB3) ... -STEP
// D12(PB4) ... -DIRECTION IN
#define BIT_SEEK_COMPLETE (1<<PC0)
#define BIT_TRACK_0 (1<<PC1)
#define BIT_READY (1<<PC2)
#define MFM_INPUT_MSK (BIT_SEEK_COMPLETE|BIT_TRACK_0|BIT_READY)
#define MFM_INPUT_PIN PINC
#define MFM_INPUT_PORT PORTC

#define BIT_HEAD_SELECT_0 (1<<PB0)
#define BIT_HEAD_SELECT_1 (1<<PB1)
#define BIT_HEAD_SELECT_2 (1<<PB2)
#define BIT_STEP (1<<PB3)
#define BIT_DIRECTION (1<<PB4)
#define MFM_HEAD_MASK (BIT_HEAD_SELECT_0|BIT_HEAD_SELECT_1|BIT_HEAD_SELECT_2)
#define MFM_OUTPUT_MASK (MFM_HEAD_MASK|BIT_STEP|BIT_DIRECTION)
#define MFM_OUTPUT_PORT PORTB
#define MFM_OUTPUT_DDR DDRB

uint8_t mfmInputReg = 0;
uint8_t mfmHeadSelect = 0;
int mfmCyl = 0;
int mfmSteps = 0;
int mfmDelta = 0;
uint32_t lastStepMillis = 0;
char buf[64];
#define INBUF_LEN 64
char inbuf[INBUF_LEN];
int inIdx = 0;
int inChar;
bool driveReady=false;

void setup()
{
 	Serial.begin(115200);
	Serial.println("running setup()!");

  MFM_INPUT_PORT |= MFM_INPUT_MSK; // PullUp-resistors on the input lines
  MFM_OUTPUT_DDR |= MFM_OUTPUT_MASK; // Configure output pins
  MFM_OUTPUT_PORT |= MFM_OUTPUT_MASK; // set all to inactive
  mfmInputReg = MFM_INPUT_PIN & MFM_INPUT_MSK;
  driveReady = ((mfmInputReg & BIT_READY) == 0);
  SendInputReg();
  delay(1);
  CheckSetUnknownCylPos();
}

void SendInputReg()
{
    sprintf(buf,"mfmInputReg:0x%02x", mfmInputReg);
    Serial.println(buf);
}

void CheckSetUnknownCylPos()
{
  if(driveReady && ((MFM_INPUT_PIN & BIT_TRACK_0) != 0) && (mfmCyl == 0) && (mfmSteps == 0))
  {
	  Serial.println("unknown cylinder position!");
    mfmCyl = 625; // unknown cylinder position!
  }
}

void loop()
{
  uint8_t inp = MFM_INPUT_PIN & MFM_INPUT_MSK;
  if(inp != mfmInputReg)
  {
    mfmInputReg = inp;
    SendInputReg();
    if(driveReady != ((mfmInputReg & BIT_READY) == 0))
    {
      driveReady = !driveReady;
      if(driveReady)
      {
        Serial.println("DRIVE READY!");
      }
      else
      {
        Serial.println("DRIVE NOT READY!");
      }
    }
  }
  if(driveReady)
  {
  	uint32_t curMillis = millis();
    if((mfmSteps > 0) && ((mfmInputReg & BIT_TRACK_0) == 0) && (mfmDelta < 0))
    {
      mfmCyl = 0;
      mfmSteps = 0;
    }
    if((mfmSteps > 0) && ((curMillis - lastStepMillis) > 2))
    {
      MFM_OUTPUT_PORT &= ~BIT_STEP;
      mfmSteps--;
      lastStepMillis = curMillis;
      mfmCyl += mfmDelta;
      MFM_OUTPUT_PORT |= BIT_STEP;
    }
    while((inChar = Serial.read()) > 0)
    {
      if((inChar == '\r') || (inChar == '\n'))
      {
        inbuf[inIdx] = 0;
        parseInBuf();
      }
      else
      {
        inbuf[inIdx++] = (char)inChar;
        if(inIdx >= INBUF_LEN)
        {
          inbuf[INBUF_LEN-1] = 0;
          Serial.println("inbuf overflow!");
          // ignore rest of buffer
          while(Serial.read() > 0)
            ;
          inIdx = 0;
        }
      }
    }
  }
}

void parseInBuf()
{
  int cylNum;
  uint8_t headNum;
  if(inbuf[0] == 's')
  {
    headNum = inbuf[1] & 0x07;
    sscanf(&inbuf[2], "%i", &cylNum);
    sprintf(buf,"head:%i, cylinder:%i (0x%04x)", headNum, cylNum, cylNum);
    Serial.println(buf);
    if(cylNum > mfmCyl)
    {
      MFM_OUTPUT_PORT &= ~BIT_DIRECTION;
      mfmDelta = 1;
      mfmSteps = cylNum - mfmCyl;
    }
    else
    {
      MFM_OUTPUT_PORT |= BIT_DIRECTION;
      mfmDelta = -1;
      mfmSteps = mfmCyl - cylNum;
    }
    if(headNum != mfmHeadSelect)
    {
      MFM_OUTPUT_PORT |= MFM_HEAD_MASK;
      MFM_OUTPUT_PORT &= ~headNum;
      mfmHeadSelect = headNum;
    }
  }
  else if (inbuf[0] == 'i')
  {
    mfmInputReg = 0xFF; // force a status report
  }
  inIdx = 0;
}
