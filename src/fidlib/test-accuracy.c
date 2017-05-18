//
//	This is a little test to compare how combined filters compare
//	to evaluating filters in their natural stages.
//

#include <stdio.h>
#include <stdlib.h>

double
process3(double val) {
   static double buf[9];
   buf[8]= buf[7]; buf[7]= buf[6]; buf[6]= buf[5]; buf[5]= buf[4]; 
   buf[4]= buf[3]; buf[3]= buf[2]; buf[2]= buf[1]; buf[1]= buf[0]; 
   buf[0]= val+7.64611284306301*buf[1]-25.5848711347952*buf[2]
     +48.9340746717192*buf[3]-58.5117578041416*buf[4]+44.7897320543968*buf[5]
     -21.4346996246982*buf[6]+5.86329783240186*buf[7]
      -0.701888838149905*buf[8];
   return 7.97006070236564e-13*buf[0]+6.37604856189252e-12*buf[1]
     +2.23161699666238e-11*buf[2]+4.46323399332476e-11*buf[3]
     +5.57904249165595e-11*buf[4]+4.46323399332476e-11*buf[5]
     +2.23161699666238e-11*buf[6]+6.37604856189252e-12*buf[7]
      +7.97006070236564e-13*buf[8];
}

double
process4(double in) {
   static float buf[9];
   float val= in;
   buf[8]= buf[7]; buf[7]= buf[6]; buf[6]= buf[5]; buf[5]= buf[4]; 
   buf[4]= buf[3]; buf[3]= buf[2]; buf[2]= buf[1]; buf[1]= buf[0]; 
   buf[0]= val+7.64611284306301*buf[1]-25.5848711347952*buf[2]
     +48.9340746717192*buf[3]-58.5117578041416*buf[4]+44.7897320543968*buf[5]
     -21.4346996246982*buf[6]+5.86329783240186*buf[7]
      -0.701888838149905*buf[8];
   return 7.97006070236564e-13*buf[0]+6.37604856189252e-12*buf[1]
     +2.23161699666238e-11*buf[2]+4.46323399332476e-11*buf[3]
     +5.57904249165595e-11*buf[4]+4.46323399332476e-11*buf[5]
     +2.23161699666238e-11*buf[6]+6.37604856189252e-12*buf[7]
      +7.97006070236564e-13*buf[8];
}


double
process1(double val) {
   static double buf1[3];
   static double buf2[3];
   static double buf3[3];
   static double buf4[3];
   buf1[2]= buf1[1]; buf1[1]= buf1[0]; 
   buf1[0]= val+1.91152821076575*buf1[1]-0.915307632868809*buf1[2];
   val= 0.000944855525763628*buf1[0]+0.00188971105152726*buf1[1]+0.000944855525763628*buf1[2];
   buf2[2]= buf2[1]; buf2[1]= buf2[0]; 
   buf2[0]= val+1.91152821076575*buf2[1]-0.915307632868809*buf2[2];
   val= 0.000944855525763628*buf2[0]+0.00188971105152726*buf2[1]+0.000944855525763628*buf2[2];
   buf3[2]= buf3[1]; buf3[1]= buf3[0]; 
   buf3[0]= val+1.91152821076575*buf3[1]-0.915307632868809*buf3[2];
   val= 0.000944855525763628*buf3[0]+0.00188971105152726*buf3[1]+0.000944855525763628*buf3[2];
   buf4[2]= buf4[1]; buf4[1]= buf4[0]; 
   buf4[0]= val+1.91152821076575*buf4[1]-0.915307632868809*buf4[2];
   val= 0.000944855525763628*buf4[0]+0.00188971105152726*buf4[1]+0.000944855525763628*buf4[2];
   return val;
}

double
process2(double in) {
   static float buf1[3];
   static float buf2[3];
   static float buf3[3];
   static float buf4[3];
   float val= in;
   buf1[2]= buf1[1]; buf1[1]= buf1[0]; 
   buf1[0]= val+1.91152821076575*buf1[1]-0.915307632868809*buf1[2];
   val= 0.000944855525763628*buf1[0]+0.00188971105152726*buf1[1]+0.000944855525763628*buf1[2];
   buf2[2]= buf2[1]; buf2[1]= buf2[0]; 
   buf2[0]= val+1.91152821076575*buf2[1]-0.915307632868809*buf2[2];
   val= 0.000944855525763628*buf2[0]+0.00188971105152726*buf2[1]+0.000944855525763628*buf2[2];
   buf3[2]= buf3[1]; buf3[1]= buf3[0]; 
   buf3[0]= val+1.91152821076575*buf3[1]-0.915307632868809*buf3[2];
   val= 0.000944855525763628*buf3[0]+0.00188971105152726*buf3[1]+0.000944855525763628*buf3[2];
   buf4[2]= buf4[1]; buf4[1]= buf4[0]; 
   buf4[0]= val+1.91152821076575*buf4[1]-0.915307632868809*buf4[2];
   val= 0.000944855525763628*buf4[0]+0.00188971105152726*buf4[1]+0.000944855525763628*buf4[2];
   return val;
}


int 
main(int ac, char **av) {
   double  in= 1.0;

   while (1) {
      printf("%-16.6g%-16.6g%-16.6g%-16.6g\n",
	     process1(in),
	     process2(in),
	     process3(in),
	     process4(in));
      in= 0.0;
   }
   return 0;
}
	     
