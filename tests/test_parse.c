#include "test.h"

/*
 * Parser contract (summary.md -> "Argument parsing & validation"):
 *   - argc must be 9 (program name + 8 mandatory arguments).
 *   - numeric args: non-empty, every char is a digit, length-bounded (overflow),
 *     then atoi; reject negatives and any non-integer.
 *   - scheduler: strcmp == "fifo" or "edf".
 *   - number_of_coders >= 1 (1 coder / 1 dongle is valid).
 *   - time values may be 0 (phase skipped; cooldown 0 = immediately available).
 *   - invalid input: usage + error to stderr, non-zero return.
 *
 * TODO: implement the parser, then fill this in. Example assertions:
 *   - "42" -> 42; "007" -> 7; "0" -> 0
 *   - reject "", "-1", "+5", "12abc", " 7", a 20-digit overflow string
 *   - scheduler "fifo"/"edf" accepted, "FIFO"/"lifo"/"" rejected
 *   - number_of_coders 0 rejected, 1 accepted
 */
void	test_parse(void)
{
}