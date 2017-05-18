//
//	For Windows, redirect the main routine to run console_main()
//	in SDL, so that we get console STDOUT and STDERR as expected.
//

extern int console_main(int, char **);

int 
main(int ac, char **av) {
   return console_main(ac, av);
}

// END //
