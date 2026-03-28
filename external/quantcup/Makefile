all:
	gcc -O3 -fpermissive -c engine.c
	gcc -O3 -fpermissive test.c engine.o -o test
	gcc -O3 -fpermissive score.c engine.o -lm -lrt -o score

clean:
	rm -f engine.o test score a.out *~
