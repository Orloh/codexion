NAME		= codexion

CC			= cc
CFLAGS		= -Wall -Wextra -Werror
CPPFLAGS	= -I include
RM			= rm -f

SRCDIR		= src
SRCS		= $(wildcard $(SRCDIR)/*.c)
OBJS		= $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean all

.PHONY: all clean fclean re
