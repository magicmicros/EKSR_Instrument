
#include "ATouch.h"

//bool ATouch::begin(TFT_eSPI *pTFT_eSPI) { _TFT_eSPI = pTFT_eSPI; return true; }

//
//  XL - GPIO 2
//  YU - GPIO 1
//  XR - GPIO 3
//  YD - GPIO 4
//
 void  ATouch::selectMeasurement(measurement m) {
   switch (m) {
     case MEASURE_Y:

      // reset input pins
      digitalWrite(IO_XR, LOW);
      digitalWrite(IO_XL, LOW);
      pinMode(IO_XL, INPUT_PULLUP);
      pinMode(IO_XR, INPUT_PULLUP);

      // set new output pins
      pinMode(IO_YD, OUTPUT);
      pinMode(IO_YU, OUTPUT);
      digitalWrite(IO_YD, HIGH);
      digitalWrite(IO_YU, LOW);

      break;

     case MEASURE_X:

      // reset input pins
      digitalWrite(IO_YU, LOW);
      digitalWrite(IO_YD, LOW);
      pinMode(IO_YU, INPUT_PULLUP);
      pinMode(IO_YD, INPUT_PULLUP);

      // set new output pins
      pinMode(IO_XL, OUTPUT);
      pinMode(IO_XR, OUTPUT);
      digitalWrite(IO_XR, HIGH);
      digitalWrite(IO_XL, LOW);

      break;
   }
 }


/***************************************************************************************
** Function name:           getTouchRaw
** Description:             read raw touch position.  Always returns true.
***************************************************************************************/
uint8_t ATouch::getTouchRaw(uint16_t *x, uint16_t *y) {
  uint16_t x1,x2,y1,y2;

  selectMeasurement(MEASURE_X);

  for (int i=0;i<3;i++) {
    x1 = analogRead(IO_YD);
    x2 = analogRead(IO_YU);
  }

  *x = (x1+x2) / 2;

  selectMeasurement(MEASURE_Y);

  for (int i=0;i<3;i++) {
    y1 = analogRead(IO_XL);
    y2 = analogRead(IO_XR);
  }

  *y = (y1+y2) / 2;

  return true;
}

/***************************************************************************************
** Function name:           getTouchRawZ
** Description:             read raw pressure on touchpad and return Z value.
***************************************************************************************/
uint16_t ATouch::getTouchRawZ(void){
  // Z sample request
  uint16_t x,y;
    selectMeasurement(MEASURE_X);
  for (int i=0;i<1;i++)
    x = analogRead(IO_YD);

  selectMeasurement(MEASURE_Y);
  for (int i=0;i<1;i++)
    y = analogRead(IO_XL);

  if ((x < 4090) && (y < 4090))
    return 1000;

  return 0;
}

/***************************************************************************************
** Function name:           validTouch
** Description:             read validated position. Return false if not pressed.
***************************************************************************************/
#define _RAWERR 20 // Deadband error allowed in successive position samples

uint8_t ATouch::validTouch(uint16_t *x, uint16_t *y, uint16_t threshold){
  uint16_t x_tmp, y_tmp, x_tmp2, y_tmp2;

  // Wait until pressure stops increasing to debounce pressure
  uint16_t z1 = 1;
  uint16_t z2 = 0;
  while (z1 > z2)
  {
    z2 = z1;
    z1 = getTouchRawZ();
    delay(1);
  }

//    Serial.print("Z = ");Serial.println(z1);

  if (z1 <= threshold) return false;

  getTouchRaw(&x_tmp,&y_tmp);

//    Serial.print("Sample 1 x,y = "); Serial.print(x_tmp);Serial.print(",");Serial.print(y_tmp);
//    Serial.print(", Z = ");Serial.println(z1);

  delay(1); // Small delay to the next sample
  if (getTouchRawZ() <= threshold) return false;

  delay(2); // Small delay to the next sample
  getTouchRaw(&x_tmp2,&y_tmp2);

//    Serial.print("Sample 2 x,y = "); Serial.print(x_tmp2);Serial.print(",");Serial.println(y_tmp2);
//    Serial.print("Sample difference = ");Serial.print(abs(x_tmp - x_tmp2));Serial.print(",");Serial.println(abs(y_tmp - y_tmp2));

  if (abs(x_tmp - x_tmp2) > _RAWERR) return false;
  if (abs(y_tmp - y_tmp2) > _RAWERR) return false;

  *x = x_tmp;
  *y = y_tmp;

  return true;
}

/***************************************************************************************
** Function name:           getTouch
** Description:             read callibrated position. Return false if not pressed.
***************************************************************************************/
#define Z_THRESHOLD 350 // Touch pressure threshold for validating touches
uint8_t ATouch::getTouch(uint16_t *x, uint16_t *y, uint16_t threshold){
  uint16_t x_tmp, y_tmp;

  if (threshold<20) threshold = 20;
  if (_pressTime > millis()) threshold=20;

  uint8_t n = 5;
  uint8_t valid = 0;
  while (n--)
  {
    if (validTouch(&x_tmp, &y_tmp, threshold)) valid++;;
  }

  if (valid<1) { _pressTime = 0; return false; }

  _pressTime = millis() + 50;

  convertRawXY(&x_tmp, &y_tmp);

  if (x_tmp >= _width || y_tmp >= _height) return false;

  _pressX = x_tmp;
  _pressY = y_tmp;
  *x = _pressX;
  *y = _pressY;
  return valid;
}

/***************************************************************************************
** Function name:           convertRawXY
** Description:             convert raw touch x,y values to screen coordinates
***************************************************************************************/
void ATouch::convertRawXY(uint16_t *x, uint16_t *y)
{
  uint16_t x_tmp = *x, y_tmp = *y, xx, yy;

  if(!touchCalibration_rotate){
    xx=(x_tmp-touchCalibration_x0)*_width/touchCalibration_x1;
    yy=(y_tmp-touchCalibration_y0)*_height/touchCalibration_y1;
    if(touchCalibration_invert_x)
      xx = _width - xx;
    if(touchCalibration_invert_y)
      yy = _height - yy;
  } else {
    xx=(y_tmp-touchCalibration_x0)*_width/touchCalibration_x1;
    yy=(x_tmp-touchCalibration_y0)*_height/touchCalibration_y1;
    if(touchCalibration_invert_x)
      xx = _width - xx;
    if(touchCalibration_invert_y)
      yy = _height - yy;
  }
  *x = xx;
  *y = yy;
}

/***************************************************************************************
** Function name:           calibrateTouch
** Description:             generates calibration parameters for touchscreen.
***************************************************************************************/
void ATouch::calibrateTouch(uint16_t *parameters, uint32_t color_fg, uint32_t color_bg, uint8_t size){
  int16_t values[] = {0,0,0,0,0,0,0,0};
  uint16_t x_tmp, y_tmp;

/*

  for(uint8_t i = 0; i<4; i++){
    _TFT_eSPI->fillRect(0, 0, size+1, size+1, color_bg);
    _TFT_eSPI->fillRect(0, _height-size-1, size+1, size+1, color_bg);
    _TFT_eSPI->fillRect(_width-size-1, 0, size+1, size+1, color_bg);
    _TFT_eSPI->fillRect(_width-size-1, _height-size-1, size+1, size+1, color_bg);

    if (i == 5) break; // used to clear the arrows

    switch (i) {
      case 0: // up left
        _TFT_eSPI->drawLine(0, 0, 0, size, color_fg);
        _TFT_eSPI->drawLine(0, 0, size, 0, color_fg);
        _TFT_eSPI->drawLine(0, 0, size , size, color_fg);
        break;
      case 1: // bot left
        _TFT_eSPI->drawLine(0, _height-size-1, 0, _height-1, color_fg);
        _TFT_eSPI->drawLine(0, _height-1, size, _height-1, color_fg);
        _TFT_eSPI->drawLine(size, _height-size-1, 0, _height-1 , color_fg);
        break;
      case 2: // up right
        _TFT_eSPI->drawLine(_width-size-1, 0, _width-1, 0, color_fg);
        _TFT_eSPI->drawLine(_width-size-1, size, _width-1, 0, color_fg);
        _TFT_eSPI->drawLine(_width-1, size, _width-1, 0, color_fg);
        break;
      case 3: // bot right
        _TFT_eSPI->drawLine(_width-size-1, _height-size-1, _width-1, _height-1, color_fg);
        _TFT_eSPI->drawLine(_width-1, _height-1-size, _width-1, _height-1, color_fg);
        _TFT_eSPI->drawLine(_width-1-size, _height-1, _width-1, _height-1, color_fg);
        break;
      }

    // user has to get the chance to release
    if(i>0) delay(1000);

    for(uint8_t j= 0; j<8; j++){
      // Use a lower detect threshold as corners tend to be less sensitive
      while(!validTouch(&x_tmp, &y_tmp, Z_THRESHOLD/2));
      values[i*2  ] += x_tmp;
      values[i*2+1] += y_tmp;
      }
    values[i*2  ] /= 8;
    values[i*2+1] /= 8;


  }


  // from case 0 to case 1, the y value changed.
  // If the measured delta of the touch x axis is bigger than the delta of the y axis, the touch and TFT axes are switched.
  touchCalibration_rotate = false;
  if(abs(values[0]-values[2]) > abs(values[1]-values[3])){
    touchCalibration_rotate = true;
    touchCalibration_x0 = (values[1] + values[3])/2; // calc min x
    touchCalibration_x1 = (values[5] + values[7])/2; // calc max x
    touchCalibration_y0 = (values[0] + values[4])/2; // calc min y
    touchCalibration_y1 = (values[2] + values[6])/2; // calc max y
  } else {
    touchCalibration_x0 = (values[0] + values[2])/2; // calc min x
    touchCalibration_x1 = (values[4] + values[6])/2; // calc max x
    touchCalibration_y0 = (values[1] + values[5])/2; // calc min y
    touchCalibration_y1 = (values[3] + values[7])/2; // calc max y
  }

  // in addition, the touch screen axis could be in the opposite direction of the TFT axis
  touchCalibration_invert_x = false;
  if(touchCalibration_x0 > touchCalibration_x1){
    values[0]=touchCalibration_x0;
    touchCalibration_x0 = touchCalibration_x1;
    touchCalibration_x1 = values[0];
    touchCalibration_invert_x = true;
  }
  touchCalibration_invert_y = false;
  if(touchCalibration_y0 > touchCalibration_y1){
    values[0]=touchCalibration_y0;
    touchCalibration_y0 = touchCalibration_y1;
    touchCalibration_y1 = values[0];
    touchCalibration_invert_y = true;
  }

  // pre calculate
  touchCalibration_x1 -= touchCalibration_x0;
  touchCalibration_y1 -= touchCalibration_y0;

  if(touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if(touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if(touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if(touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  // export parameters, if pointer valid
  if(parameters != NULL){
    parameters[0] = touchCalibration_x0;
    parameters[1] = touchCalibration_x1;
    parameters[2] = touchCalibration_y0;
    parameters[3] = touchCalibration_y1;
    parameters[4] = touchCalibration_rotate | (touchCalibration_invert_x <<1) | (touchCalibration_invert_y <<2);
  }
  */
}


/***************************************************************************************
** Function name:           setTouch
** Description:             imports calibration parameters for touchscreen.
***************************************************************************************/
void ATouch::setTouch(uint16_t *parameters){
  touchCalibration_x0 = parameters[0];
  touchCalibration_x1 = parameters[1];
  touchCalibration_y0 = parameters[2];
  touchCalibration_y1 = parameters[3];

  if(touchCalibration_x0 == 0) touchCalibration_x0 = 1;
  if(touchCalibration_x1 == 0) touchCalibration_x1 = 1;
  if(touchCalibration_y0 == 0) touchCalibration_y0 = 1;
  if(touchCalibration_y1 == 0) touchCalibration_y1 = 1;

  touchCalibration_rotate = parameters[4] & 0x01;
  touchCalibration_invert_x = parameters[4] & 0x02;
  touchCalibration_invert_y = parameters[4] & 0x04;
}
