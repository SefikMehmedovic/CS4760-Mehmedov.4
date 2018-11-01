#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
  int input, s, l, t;
  while ((input = getopt (argc, argv, "hs:l:t:")) != -1)
	
	{
	 switch(input)
	 {
	case 'h':

		printf("\nTo run the program use ./oss\n");
		printf("-h is for help and all options\n ");
	    	printf("-s x where x is the max number of user process spawned (default 5)\n");
       		printf("-l filename is the name of the log file\n");
        	printf("-t z is where z is the time in seconds when master will terminate itself");
		printf(" and all children(default 2 seconds)\n");
       		printf("Example: ./oss -s 10 (runs oss with 10 child process)\n");
    	break;


	case 's':
       		 s = atoi(optarg);
            if(s > 20)
            {
              printf("\nS can not be greater than 20. S was set to 20\n");
              s = 20;
            }
	   else if(s == 0)
		{
     printf("\nS is set to default value of 5\n");
		 	s = 5;
		}
        	 printf("case s: %d\n", s);
     	break;
    
	case 'l':
		printf("case l input file");
	break;

	case 't':
		t = atoi(optarg);
		printf("case t: %d",t);
		if(t == 0 || t <= 0)
		{
       printf("\nT value is set to default value of 2\n");
			t = 2;
		}
	break;

   	case '?':
  	 fprintf(stderr, "Unknown option %c\n", optopt);
 	exit(1);   	
	break;
    default:
      //exit program incorrect options
     // exit(1);
    return 1;
	
	 }

}
return 0;//main
}//main