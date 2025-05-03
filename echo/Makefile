# 컴파일러 및 플래그 설정
CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lpthread  # 만약 pthread 사용 시 필요

# 모든 실행 파일 목록
TARGETS = echoclient echoserveri echoservers
# TARGETS = echo echoclient echoserveri echoservers

# 기본 타겟: 모든 실행 파일 컴파일
all: $(TARGETS)

# 공통 객체 파일 (csapp.o)
OBJECTS = csapp.o

# 각 실행 파일 생성 규칙
echo: echo.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

echoclient: echoclient.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

echoserveri: echoserveri.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

echoservers: echoservers.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# 객체 파일 생성 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 정리 타겟
clean:
	rm -f $(OBJECTS) $(TARGETS)