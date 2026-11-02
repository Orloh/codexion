NAME		= codexion

CC			= cc
CFLAGS		= -Wall -Wextra -Werror -pthread
CPPFLAGS	= -I include
RM			= rm -f

SRCDIR		= src
SRCS		= $(SRCDIR)/main.c \
			  $(SRCDIR)/init.c \
			  $(SRCDIR)/cleanup.c \
			  $(SRCDIR)/parse.c \
			  $(SRCDIR)/heap.c \
			  $(SRCDIR)/arbiter.c \
			  $(SRCDIR)/dongle_pair.c \
			  $(SRCDIR)/coder.c \
			  $(SRCDIR)/monitor.c \
			  $(SRCDIR)/log.c \
			  $(SRCDIR)/time.c
OBJS		= $(SRCS:.c=.o)

TESTDIR		= tests
TEST_SRCS	= $(TESTDIR)/test_main.c \
			  $(TESTDIR)/test_heap.c \
			  $(TESTDIR)/test_parse.c \
			  $(TESTDIR)/test_log.c \
			  $(TESTDIR)/test_init.c \
			  $(TESTDIR)/test_arbiter.c \
			  $(TESTDIR)/test_coder.c \
			  $(TESTDIR)/test_monitor.c \
			  $(TESTDIR)/test_dongle_pair.c
TEST_OBJS	= $(TEST_SRCS:.c=.o) $(SRCDIR)/heap.o $(SRCDIR)/parse.o \
			  $(SRCDIR)/log.o $(SRCDIR)/init.o $(SRCDIR)/cleanup.o \
			  $(SRCDIR)/arbiter.o $(SRCDIR)/time.o $(SRCDIR)/coder.o \
			  $(SRCDIR)/monitor.o $(SRCDIR)/dongle_pair.o
TEST_BIN	= test_codexion

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Unit tests (all modules). Link the modules under test via TEST_OBJS.
test: $(TEST_BIN)
	./$(TEST_BIN)

test-integration: $(NAME)
	python3 tests/test_integration.py ./$(NAME)

test-scenarios: $(NAME)
	bash tests/test_scenarios.sh ./$(NAME)

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $(TEST_BIN)

$(TESTDIR)/%.o: $(TESTDIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TEST_OBJS)

fclean: clean
	$(RM) $(NAME) $(TEST_BIN)

re: fclean all

.PHONY: all clean fclean re test test-integration test-scenarios