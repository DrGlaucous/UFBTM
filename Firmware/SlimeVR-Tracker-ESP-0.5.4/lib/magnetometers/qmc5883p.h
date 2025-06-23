#define QMP_DEVADDR         0x2C
#define QMP_RA_DATA         0x01
#define QMP_RA_CONTROL      0x0A
#define QMP_RA_RESET        0x0B
#define QMP_RA_CONTROL2     0x0B


//////////////////config register 1 (0x0A)

//operation mode
#define QMP_CFG_MODE_SUSPEND    0b00
#define QMP_CFG_MODE_NORMAL     0b01
#define QMP_CFG_MODE_SINGLE     0b10
#define QMP_CFG_MODE_CONT       0b11

//output data rate 
#define QMP_CFG_ODR_10HZ        0b00 << 2
#define QMP_CFG_ODR_50HZ        0b10 << 2
#define QMP_CFG_ODR_100HZ       0b01 << 2
#define QMP_CFG_ODR_200HZ       0b00 << 2

//oversampling rate
#define QMP_CFG_OVR_SMPL8       0b00 << 4
#define QMP_CFG_OVR_SMPL4       0b00 << 4
#define QMP_CFG_OVR_SMPL2       0b00 << 4
#define QMP_CFG_OVR_SMPL1       0b00 << 4

//downsampling rate
#define QMP_CFG_DOWN_SMPL1      0b00 << 6
#define QMP_CFG_DOWN_SMPL2      0b00 << 6
#define QMP_CFG_DOWN_SMPL4      0b00 << 6
#define QMP_CFG_DOWN_SMPL8      0b00 << 6


//////////////////config register 2 (0x0B) (everything below)
//reset address
#define QMP_CFG_SOFT_RESET      0b1 << 7

//gauss setting
#define QMP_CFG_RNG_2G          0b11 << 2
#define QMP_CFG_RNG_8G          0b10 << 2
#define QMP_CFG_RNG_12G         0b01 << 2
#define QMP_CFG_RNG_30G         0b00 << 2






