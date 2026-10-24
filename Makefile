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
			  $(SRCDIR)/coder.c \
			  $(SRCDIR)/monitor.c \
			  $(SRCDIR)/log.c
OBJS		= $(SRCS:.c=.o)

TESTDIR		= tests
TEST_SRCS	= $(TESTDIR)/test_main.c \
			  $(TESTDIR)/test_heap.c \
			  $(TESTDIR)/test_parse.c
TEST_OBJS	= $(TEST_SRCS:.c=.o) $(SRCDIR)/heap.o
TEST_BIN	= test_codexion

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

# Unit tests (heap + parser). Standalone for now: they only link the test
# sources. Once the heap/parser modules exist, add their objects to TEST_OBJS.
test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $(TEST_BIN)

$(TESTDIR)/%.o: $(TESTDIR)/%.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS) $(TEST_OBJS)

fclean: clean
	$(RM) $(NAME) $(TEST_BIN)

re: fclean all

.PHONY: all clean fclean re test