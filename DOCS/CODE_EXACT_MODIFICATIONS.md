# 🛠️ PLAN DÉTAILLÉ D'IMPLÉMENTATION - Code Exact

**Ce document contient le code exact à ajouter/remplacer**

---

## MODIFICATION 1: `utils.c` - event_to_str()

**Fichier:** `/home/n43/Téléchargements/drivers-and-interrupts/old/utils.c`  
**Ligne:** ~190 (fonction `event_to_str`)  
**Temps:** 5 minutes

### AVANT (❌ Actuel)
```c
char *event_to_str(event_struct event)
{
	char *output_str = kmalloc(69420, GFP_KERNEL);
	time64_t time;
	struct tm time_struct;

	time = event.time;
	time64_to_tm(time, 0, &time_struct);
	if (!output_str)
		return output_str;
	if (event.is_pressed){
		sprintf(output_str, "[%d:%d:%d] %s(0X%X) Pressed\n", 
			time_struct.tm_hour, time_struct.tm_min, time_struct.tm_sec, 
			event.name, event.scan_code);
	}
	else{
		sprintf(output_str, "[%d:%d:%d] %s(0X%X) Released\n", 
			time_struct.tm_hour, time_struct.tm_min, time_struct.tm_sec, 
			event.name, event.scan_code);
	}
	return output_str;
}
```

### APRÈS (✅ Correct)
```c
char *event_to_str(event_struct event)
{
	char *output_str = kmalloc(256, GFP_KERNEL);
	time64_t time;
	struct tm time_struct;

	time = event.time;
	time64_to_tm(time, 0, &time_struct);
	if (!output_str)
		return output_str;
	
	snprintf(output_str, 256, "%02d:%02d:%02d %s(%d) %s\n",
		time_struct.tm_hour, 
		time_struct.tm_min, 
		time_struct.tm_sec,
		event.name, 
		event.scan_code,
		event.is_pressed ? "Pressed" : "Released");
	
	return output_str;
}
```

**Changements clés:**
- ✅ `snprintf()` au lieu `sprintf()`
- ✅ Limite 256 bytes
- ✅ `%02d` pour padding
- ✅ `%d` pour code décimal
- ✅ Ternaire pour "Pressed"/"Released"
- ✅ Format exact: `HH:MM:SS Name(code) State`

---

## MODIFICATION 2: `device.c` - ft_module_keyboard_read()

**Fichier:** `/home/n43/Téléchargements/drivers-and-interrupts/old/device.c`  
**Fonction:** `ft_module_keyboard_read()`  
**Temps:** 15 minutes

### Ajouter constante en haut du fichier
```c
#define READ_BUFFER_MAX 8192
```

### AVANT (❌ Dangereux)
```c
static ssize_t ft_module_keyboard_read(struct file *file, char *buff, size_t, loff_t * offset)
{
	struct list_head *head_ptr;
	struct event_struct *entry;
	char *output_str;
	char *temp_str;
	int did_not_cpy;

	ft_log("Misc device read");
	spin_lock(&devfile_io_spinlock);
	output_str = kmalloc(69420 * 42, GFP_KERNEL);
	output_str[0] = 0;
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list))
	{
		entry = list_entry(head_ptr, struct event_struct, list);

		temp_str = event_to_str(*entry);
		strcpy(output_str + strlen(output_str), temp_str);
		kfree(temp_str);

		head_ptr = head_ptr->next;
	}
	spin_unlock(&devfile_io_spinlock);
	if (*offset == strlen(output_str))
	{
		kfree(output_str);
		return 0;
	}

	did_not_cpy = copy_to_user(buff, output_str, strlen(output_str));
	*offset = strlen(output_str);
	return strlen(output_str) - did_not_cpy;
}
```

### APRÈS (✅ Sécurisé)
```c
static ssize_t ft_module_keyboard_read(struct file *file, char *buff, size_t count, loff_t * offset)
{
	struct list_head *head_ptr;
	struct event_struct *entry;
	char *output_str;
	char *temp_str;
	size_t current_len = 0;
	size_t to_copy;
	int did_not_cpy;

	ft_log("Misc device read");
	
	// Allocation sécurisée
	output_str = kmalloc(READ_BUFFER_MAX, GFP_KERNEL);
	if (!output_str) {
		ft_warn("Failed to allocate read buffer");
		return -ENOMEM;
	}
	output_str[0] = '\0';

	// Protéger accès liste
	spin_lock(&devfile_io_spinlock);
	
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list) && 
	       current_len < READ_BUFFER_MAX - 256)
	{
		entry = list_entry(head_ptr, struct event_struct, list);
		temp_str = event_to_str(*entry);
		
		// Vérifier overflow AVANT concat
		size_t needed = strlen(temp_str);
		if (current_len + needed >= READ_BUFFER_MAX) {
			kfree(temp_str);
			break;  // Stop si pas assez de place
		}
		
		strncat(output_str, temp_str, READ_BUFFER_MAX - current_len - 1);
		current_len = strlen(output_str);
		kfree(temp_str);

		head_ptr = head_ptr->next;
	}
	spin_unlock(&devfile_io_spinlock);
	
	// Gérer offset - EOF check
	if (*offset >= current_len)
	{
		kfree(output_str);
		return 0;  // EOF
	}

	// Calculer combien copier
	size_t remaining = current_len - *offset;
	to_copy = remaining < count ? remaining : count;

	// Copier depuis offset
	did_not_cpy = copy_to_user(buff, output_str + *offset, to_copy);
	
	// Vérifier erreur copy_to_user
	if (did_not_cpy > 0) {
		ft_warn("copy_to_user failed");
		kfree(output_str);
		return -EFAULT;
	}

	// Update offset
	*offset += (to_copy - did_not_cpy);
	
	kfree(output_str);
	return to_copy - did_not_cpy;
}
```

**Changements clés:**
- ✅ Buffer limité à 8 KB
- ✅ Vérifier `kmalloc()` retour
- ✅ Utiliser `strncat()` avec limite
- ✅ Vérifier overflow AVANT
- ✅ Offset handling correct
- ✅ Copier depuis offset
- ✅ Gérer lectures fragmentées
- ✅ Vérifier `copy_to_user()` retour

---

## MODIFICATION 3: `interrupt.c` - read_key()

**Fichier:** `/home/n43/Téléchargements/drivers-and-interrupts/old/interrupt.c`  
**Fonction:** `read_key()`  
**Temps:** 5 minutes

### AVANT (❌ Race condition)
```c
void read_key(struct work_struct *workqueue)
{
	int scancode = inb(KB_PORT);
	event_struct *event;

	queue_data *q_data = container_of(workqueue, queue_data, worker);

	// preprocessing
	spin_lock(&q_data_spinlock);
	q_data->is_caps = &is_caps;
	q_data->is_shift = &is_shift;
	spin_unlock(&q_data_spinlock);

	// event creation
	event = ft_generate_event(*q_data, scancode);

	// event storing
	list_add_tail(&(event->list), &(q_data->driver.events_head->list));

	// post processing (shift, caps)
	spin_lock(&q_data_spinlock);
	// ...
}
```

### APRÈS (✅ Thread-safe)
```c
void read_key(struct work_struct *workqueue)
{
	int scancode = inb(KB_PORT);
	event_struct *event;

	queue_data *q_data = container_of(workqueue, queue_data, worker);

	// preprocessing
	spin_lock(&q_data_spinlock);
	q_data->is_caps = &is_caps;
	q_data->is_shift = &is_shift;
	spin_unlock(&q_data_spinlock);

	// event creation
	event = ft_generate_event(*q_data, scancode);

	// event storing - AVEC PROTECTION
	spin_lock(&devfile_io_spinlock);
	list_add_tail(&(event->list), &(q_data->driver.events_head->list));
	g_driver->total_events++;  // Bonus: compter les événements
	spin_unlock(&devfile_io_spinlock);

	// post processing (shift, caps)
	spin_lock(&q_data_spinlock);
	// ... reste du code ...
}
```

**Changements clés:**
- ✅ `spin_lock(&devfile_io_spinlock)` AVANT `list_add_tail()`
- ✅ Incrémenter `total_events`
- ✅ `spin_unlock()` APRÈS
- ✅ Même spinlock que dans device.c

---

## MODIFICATION 4: `tmpfile.c` - Nouvelle fonction

**Fichier:** `/home/n43/Téléchargements/drivers-and-interrupts/old/tmpfile.c`  
**Nouvelle fonction:** `ft_log_tmpfile_with_stats()`  
**Temps:** 20 minutes

### Ajouter cette fonction AVANT `ft_log_tmpfile()`:
```c
/**
 * ft_log_tmpfile_with_stats - Write kernel log to /tmp with statistics
 * 
 * Affiche un résumé user-friendly du log clavier dans /tmp
 * Sans afficher tous les événements (seulement les 100 derniers)
 */
void ft_log_tmpfile_with_stats(void)
{
	struct list_head *head_ptr;
	struct event_struct *entry;
	char *output_str;
	char *stats_str;
	int total_events = 0;
	int total_alpha = 0;
	int count = 0;

	// Allouer buffers
	output_str = kmalloc(2048, GFP_KERNEL);
	stats_str = kmalloc(512, GFP_KERNEL);
	if (!output_str || !stats_str) {
		ft_warn("Memory allocation failed for stats");
		if (output_str) kfree(output_str);
		if (stats_str) kfree(stats_str);
		return;
	}

	// Compter les événements
	spin_lock(&devfile_io_spinlock);
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list)) {
		entry = list_entry(head_ptr, struct event_struct, list);
		total_events++;
		
		// Compter touches alphanumériques pressées
		if (entry->is_pressed && 
		    entry->ascii_value > 32 && 
		    entry->ascii_value < 127) {
			total_alpha++;
		}
		head_ptr = head_ptr->next;
	}

	// Écrire header avec stats
	snprintf(stats_str, 512,
		"=== KEYBOARD LOG ===\n"
		"Total Events: %d\n"
		"Alphanumeric Keys: %d\n"
		"=== EVENTS ===\n",
		total_events, total_alpha);

	ft_write_tmpfile(stats_str, strlen(stats_str));

	// Écrire événements (limiter à 100 derniers)
	count = 0;
	head_ptr = g_driver->events_head->list.next;
	while (head_ptr != &(g_driver->events_head->list) && count < 100) {
		entry = list_entry(head_ptr, struct event_struct, list);
		char *event_line = event_to_str(*entry);
		if (event_line) {
			ft_write_tmpfile(event_line, strlen(event_line));
			kfree(event_line);
		}
		head_ptr = head_ptr->next;
		count++;
	}
	spin_unlock(&devfile_io_spinlock);

	// Footer
	snprintf(stats_str, 512,
		"=== END LOG (Showing last %d of %d events) ===\n",
		count, total_events);
	ft_write_tmpfile(stats_str, strlen(stats_str));

	// Cleanup
	kfree(output_str);
	kfree(stats_str);
	
	// Fermer le fichier
	if (tmpfile) {
		filp_close(tmpfile, NULL);
		tmpfile = NULL;
	}
}
```

**Changements clés:**
- ✅ Affiche header
- ✅ Affiche stats (total + alphanumeric)
- ✅ Affiche jusqu'à 100 événements
- ✅ Affiche footer avec limite
- ✅ "User friendly" comme demandé
- ✅ Spinlock protégé

---

## MODIFICATION 5: `main.c` - cleanup_module()

**Fichier:** `/home/n43/Téléchargements/drivers-and-interrupts/old/main.c`  
**Fonction:** `cleanup_module()`  
**Temps:** 2 minutes

### AVANT
```c
void cleanup_module(void)
{
	ft_log("Cleaning up module");

	// print everything here
	ft_log_tmpfile();

	ft_deregister_interrupt();
	ft_destroy_misc_device();
	ft_deregister_usb();
	ft_free_driver(g_driver);

}
```

### APRÈS
```c
void cleanup_module(void)
{
	ft_log("Cleaning up module");

	// print everything here with stats
	ft_log_tmpfile_with_stats();  // ✅ NOUVELLE FONCTION

	ft_deregister_interrupt();
	ft_destroy_misc_device();
	ft_deregister_usb();
	ft_free_driver(g_driver);
	
	ft_log("Module cleanup complete");
}
```

**Changement:**
- ✅ Appeler `ft_log_tmpfile_with_stats()` au lieu `ft_log_tmpfile()`

---

## MODIFICATION 6: `42kb.h` - Prototypes

**Fichier:** `/home/n43/Téléchargements/drivers-and-interrupts/old/42kb.h`  
**Ligne:** À la fin du fichier  
**Temps:** 2 minutes

### Ajouter avant le dernier `#endif`:
```c
// tmpfile functions
void ft_log_tmpfile_with_stats(void);

// Constants
#define READ_BUFFER_MAX 8192
```

---

## ✅ RÉSUMÉ DES MODIFICATIONS

| Fichier | Fonction | Change | Ligne |
|---------|----------|--------|-------|
| `utils.c` | event_to_str() | Format fix | ~190 |
| `device.c` | ft_module_keyboard_read() | Buffer safe | ~50 |
| `interrupt.c` | read_key() | Add spinlock | ~35 |
| `tmpfile.c` | NEW | Stats logging | new |
| `main.c` | cleanup_module() | Call new func | ~160 |
| `42kb.h` | - | Add prototype | end |

---

## 🧪 APRÈS CHAQUE MODIFICATION

```bash
# Compiler
make clean && make

# Vérifier errors
make 2>&1 | grep -i error

# Vérifier warnings (optional)
make 2>&1 | grep -i warning
```

---

## 🚀 ORDRE RECOMMANDÉ

1. **Modifier `42kb.h`** (2 min) - Ajouter prototypes & defines
2. **Modifier `utils.c`** (5 min) - Fixer format
3. **Modifier `interrupt.c`** (5 min) - Ajouter spinlock
4. **Modifier `device.c`** (15 min) - Buffer sécurisé
5. **Modifier `tmpfile.c`** (20 min) - Nouvelle fonction
6. **Modifier `main.c`** (2 min) - Appeler nouvelle fonction
7. **Compiler & test** (30 min)

**Total: ~1.5 heures**

---

## 💡 TIPS

- Faire une modification à la fois
- Compiler après CHAQUE modification
- Tester après TOUTES les modifications
- Si compile échoue, revenir à la modif précédente
- Use `git diff` pour voir les changements exacts

---

## 🎯 PRÊT?

Si suivre ce plan exactement, tout devrait fonctionner! 🚀

