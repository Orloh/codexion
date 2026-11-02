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

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

test: $(NAME)
	./tests.sh ./$(NAME)

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re test
