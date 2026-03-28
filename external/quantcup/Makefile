all:
	gcc -O3 -fpermissive -c voyager.c
	gcc -O3 -fpermissive test.c voyager.o -o test
	gcc -O3 -fpermissive score.c voyager.o -lm -lrt -o score

clean:
	rm -f voyager.o test score a.out *~
