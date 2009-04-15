/* buffer.h	8/19/1995
 */

/* Copyright (c) 1995,1996 Sascha Demetrio
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *    If you modify any part of HEXER and resitribute it, you must add
 *    a notice to the `README' file and the modified source files containing
 *    information about the  changes you made.  I do not want to take
 *    credit or be blamed for your modifications.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *    If you modify any part of HEXER and resitribute it in binary form,
 *    you must supply a `README' file containing information about the
 *    changes you made.
 * 3. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * HEXER WAS DEVELOPED BY SASCHA DEMETRIO.
 * THIS SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT.
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT
 * NOT MAKE USE OF THIS WORK.
 *
 * DISCLAIMER:
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _BUFFER_H_
#define _BUFFER_H_

/* Optionen fuer den Typ 'Buffer'. Beschreibung siehe Typ 'BufferOptions'.
 */
#define B_READ_ONLY     0x0001
#define B_PAGING        0x0100
#define B_LOAD_FILE     0x0200

/* Die Struktur 'BufferOptions' beschreibt die Optionen, mit denen ein Buffer
 * erzeugt werden soll. Die Defaults werden in einer globalen Variablen
 * 'b_default_options' (Modul buffer.o) festgelegt. Die Initialisierung
 * erfolgt mit Werten, die in 'defs.h' definiert sind.
 * Diese globale Variable *kann* vom user veraendert werden;
 * ich empfehle, dies nur in der Funtion 'main()' zu tun, um seltsame
 * Seiteneffekte zu vermeiden.
 */
struct BufferOptions
{
  unsigned long blocksize;
    /* Die Defaultgroesse eines Blockes in einem Buffer.
     */
  unsigned short opt;
    /* Jedes Bit steht fuer eine Option.
     *
     * Bit | Define        | Beschreibung
     * ----+---------------+----------------------------------------------
     * 0   | B_READ_ONLY   | Falls auf einen read-only Buffer ein
     *     |               | Schreibzugriff stattfindet, wird ein
     *     |               | assertation failure ausgeloest.
     *     |               | Eine Optimierung der Lesezugriffe auf einen
     *     |               | read-only Buffer mit Paging ist geplant.
     * 8   | B_PAGING      | Bloecke werden bei Bedarf auf die Platte
     *     |               | ausgelagert. (NIY)
     * 9   | B_LOAD_FILE   | Falls 'B_PAGING' aktiv ist, wird eine Datei
     *     |               | nach dem Oeffnen so weit wie moeglich
     *     |               | 'max_blocks' eingelesen. (NIY)
     */
  long max_blocks;
    /* Falls 'B_PAGING' aktiviert ist, gibt 'max_blocks' die maximale
     * Anzahl Bloecke an, die der Buffer im Speicher halten darf.
     * -1 bedeutet unbegrenzt.
     * NOTE: Eine unbegrenzte Anzahl von Bloecken zuzulassen kann durchaus
     *   sinnvoll sein, da wenn 'B_LOAD_FILE' *nicht* gesetzt ist, nur die
     *   Bloecke geladen werden, auf die tatsaechlich Lesezugriffe
     *   stattfinden (demand loading). Dies kann insbesondere fuer grosse
     *   read-only Buffer sinnvoll sein.
     * NOTE: NIY
     */
};
/* BufferOptinons */
extern struct BufferOptions b_default_options;

/* Der Typ 'BufferBlock' ist der Listenelement-Typ des Typs 'Buffer'.
 */
typedef struct BufferBlock
{
  struct BufferBlock *next_block;
    /* Zeiger auf das naechste Listenelement.
     */
  char *data;
    /* Zeiger auf den Datenblock. Die Groesse des Datenblocks wird in der
     * Komponente 'blocksize' des Typs 'Buffer' gespeichert.
     */
}
BufferBlock;

/* Der Typ 'Buffer' ist eine verkette Liste von Speicherbloecken vom Typ
 * 'BufferBlock'.
 */
typedef struct Buffer
{
  BufferBlock *first_block;
    /* Zeiger auf das erste Listenelement.
     */
  unsigned long size;
    /* Anzahl der Bytes, die im Buffer gespeichert sind.
     */
  unsigned long blocksize;
    /* Groesse eines Datenblocks. (-> Typ 'BufferBlock')
     */
  int read_only;
    /* Bool. Gibt an, ob es sich um einen read-only Buffer handelt.
     * Schreibende Funktionen enthalten ein
     *   assert(!buffer -> read_only);
     */
  int modified;
    /* Bool.  Wird bei jedem Schreibzugriff auf 1 gesetzt.
     */

  /* NOTE: Der Grund dafuer, dass die Struktur nicht einfach eine
   *   Komponente 'BufferOptions options;' enthaelt ist der, dass es zu
   *   unuebersichtlich wuerde, wenn man bei jeder Abrage ein Bit in
   *   einer Komponente einer Unterstruktur testen muesste...
   */
} 
Buffer;

  extern long
count_lines( /* char *source, long count */ );
  /* Liest 'count' Zeichen aus 'source' und liefert die Anzahl der
   * Zeilentrenner.
   */

  extern BufferBlock *
new_buffer_block( /* unsigned long blocksize, char *data */ );
  /* Konstruktor fuer 'BufferBlock'.
   * blocksize:
   *   Die Groesse eines Datenblocks. Dieser Wert ist nur relevant, wenn
   *   Speicherplatz fuer einen Datenblock reserviert werden soll.
   * data:
   *   Zeiger auf den Datenblock, der von dem erzeugten BufferBlock
   *   representiert werden soll. (VORSICHT: Der Datenblock wird *nicht*
   *   kopiert.) Wenn 'NULL' uebergeben wird, wird der Speicher fuer
   *   den Datenblock via 'malloc()' reserviert. Nur in diesem Fall ist
   *   die Angabe der 'blocksize' relevant.
   * Rueckgabe:
   *   Die Funktion liefert einen Pointer auf den erzeugten BufferBlock.
   *   Im Fehlerfall wird 'NULL' zurueckgeliefert.
   */

  extern void
delete_buffer_block( /* BufferBlock *block */ );
  /* Destuktor fuer 'BufferBlock'.
   */

  extern Buffer *
new_buffer( /* struct BufferOptions *options */ );
  /* Konstruktor fuer 'Buffer'. Es wird ein Buffer der Laenge 0 erzeugt.
   * options:
   *   Zeiger auf eine 'BufferOptions'-Struktur, die die Optionen zum
   *   erzeugen des Buffers enthaelt. Fuer 'options == NULL' werden die
   *   Optionen aus 'b_default_options' gelesen.
   * Rueckgabe:
   *   Die Funktion liefert einen Zeiger auf den erzeugten Buffer.
   *   Im Fehlerfall wird 'NULL' geliefert.
   */

  extern
delete_buffer( /* Buffer *buffer */ );
  /* Destruktor fuer 'Buffer'.
   */

  extern
copy_buffer( /* Buffer *target, Buffer *source */ );
  /* Kopierfunktion fuer 'Buffer'. Es wird nur eine *inhaltliche*n Kopie
   * des Buffers angelegt, d.h. die Blockgroessen von '*target' und
   * '*source' bleiben unveraendert.
   * target:
   *   Zeiger auf den Zielbuffer.
   * source:
   *   Zeiger auf den Quellbuffer.
   * Rueckgabe:
   *   Immer 0. Eine Fehlerabfrage ist noch nicht implementiert.
   *   (Im Fehlerfall sollte -1 geliefert werden.)
   * BEMERKUNG: Der Zielbuffer '*target' muss existieren, d.h. die Funktion
   *   darf nicht als Konstruktor missbraucht werden.
   */

  extern void
clear_buffer( /* Buffer *buffer */ );
  /* Loescht den Buffer '*buffer', d.h. buffer zeigt nach dem Aufruf auf
   * einen leeren Buffer. (Diese Funktion ruft einfach nur 'b_clear()' auf.
   */

  extern BufferBlock *
find_block( /* Buffer *buffer, unsigned long position */ );
  /* Liefert einen Zeiger auf den BufferBlock des Buffers '*buffer', in dem
   * sich das Byte mit der Position 'position' befindet. Falls sich die
   * Position 'position' jenseits des Bufferendes befindet, liefert die
   * Funktion 'NULL'.
   */

/* Die folgenden Methoden koennen als 'public' im Sinne von C++
 * verstanden werden. Die Blockstruktur eines Buffers wird durch diese
 * Methoden transparent gemacht. Per Konvention sollen die Funktionsnamen
 * aller dieser Methoden mit 'b_' beginnen, weiterhin soll das erste
 * Argument immer ein Zeiger auf den betreffenden Buffer sein. Diese
 * Konventionen sollten auch von anderen Modulen, die Methoden fuer
 * diesen Buffer zur Verfuegung stellen, eingehalten werden.
 */

  extern
b_set_size( /* Buffer *buffer, unsigned long size */ );
  /* Aendert die Groesse eines Buffers '*buffer' auf 'size'. Falls der
   * Buffer nach der Aenderung groesser als vorher ist, ist der Inhalt
   * der neu hinzugekommenen Bytes undefiniert. Im Fehlerfall wird -1
   * geliefert, 0 sonst.
   */

  extern long
b_read( /* Buffer *buffer, char *target, long position, long count */ );
  /* Liest 'count' Bytes ab der Position 'position' nach '*target'.
   * Zurueckgeliefert wird die Anzahl der tatsaechlich gelesenen Bytes.
   * Diese Zahl kann kleiner sein, als 'count', wenn die Position
   * 'position + count' jenseits vom Bufferende lieget. Falls schon
   * 'position' die Laenge des Buffers uebersteigt, werden 0 Bytes gelesen.
   * Es ist keine Fehlerabfrage implementiert.
   */

  extern long
b_write( /* Buffer *buffer, char *source, long position, long count */ );
  /* Schreibt 'count' Bytes aus '*source' ab Position 'position'.
   * Die im Buffer befindlichen Daten werden ueberschrieben (kein Insert).
   * Die Funktion schreibt nicht ueber das Ende des Buffers hinaus (die
   * Funktion 'b_write_append' tut dies), zurueckgeliefert wird die
   * Anzahl des geschriebenen Bytes. Falls 'position' die Groesse des
   * Buffers uebersteigt, wird nichts geschrieben.
   * Eine Fehlerabfrage ist nicht implementiert.
   */

  extern long
b_write_append( /* Buffer *buffer, char *source,
                   long position, long count */ );
  /* Wie 'b_write', nur dass auch an Positionen jenseits vom Bufferende
   * geschrieben werden kann. In diesem Fall ist der Inhalt des Buffers
   * zwischen dem vorherigen Bufferende und der Position 'position'
   * undefiniert (diese Funktion ist also mit Vorsicht zu geniessen :-).
   * Im Fehlerfall wird -1 zurueckgeliefert, 'count' sonst.
   */

  extern long
b_append( /* Buffer *buffer, char *source, long count */ );
  /* Haengt `count' Bytes aus `source' an das Bufferende an.  Im Fehlerfall
   * wird -1 zurueckgeliefert, `count' sonst.
   */

  extern long
b_fill( /* Buffer *buffer, char c, long position, long count */ );
  /* Wie 'b_write', nur dass der angegebene Speicherbereich mit dem Zeichen
   * 'c' beschrieben wird. Die Funktion schreibt nicht ueber das Ende des
   * Buffers hinaus und liefert die Anzahl der geschriebenen Zeichen zurueck.
   * Eine Fehlerabfrage ist nicht implementiert.
   */

  extern long
b_fill_append( /* Buffer *buffer, char c, long position, long count */ );
  /* Wie 'b_fill', es kann aber auch an Positionen jenseits des Bufferendes
   * geschrieben werden. Der Inhalt des Buffers zwischen vorherigem Bufferende
   * und 'position' ist undefiniert. Im Fehlerfall wird -1 geliefert,
   * 'count' sonst.
   */

  extern long
b_count_lines( /* Buffer *buffer, long position, long count */ );
  /* Liefert die Anzahl der Zeilentrenner im angegebenen Bereich.
   */

  extern long
b_insert( /* Buffer *buffer, long position, long count */ );
  /* Vergroessert den Buffer um 'count' Bytes. Ab der Position 'position'
   * wird eine Luecke der Groesse 'count' geschaffen, indem der Inhalt des
   * Buffers ab dieser Position um 'count' Bytes verschoben wird.
   * Der Inhalt dieser Luecke ist dann undefiniert. Die Funktion liefert
   * -1 im Fehlerfall und 'count' sonst.
   */

  extern long
b_delete( /* Buffer *buffer, long position, long count */ );
  /* Schneidet ab der Position 'position' 'count' Bytes aus dem Buffer
   * heraus. Der Buffer wird dadurch um 'count' Bytes kleiner. Die Funktion
   * liefert -1 im Fehlerfall und 'count' sonst.
   */

  extern long
b_copy( /* Buffer *target_buffer, Buffer *source_buffer,
	   long target_position, long source_position, long count */ );
  /* Kopiert aus dem Buffer '*source_buffer' ab der Position 'source_position'
   * 'count' Bytes in den Buffer '*target_buffer' an die Position
   * 'target_position'. Die Funktion schreibt nicht ueber das Ende des
   * '*target_buffer' hinaus. Zurueckgeliefert wird die Anzahl der kopierten
   * Bytes und -1 im Fehlerfall.
   * BUGS: Der Rueckgabewert ist nicht korrekt.
   */

  extern long
b_copy_forward( /* Buffer *buffer,
                   long target_position, long source_position, long count */ );
  /* Kopiert innerhalb eines Buffers 'count' Bytes ab 'source_position'
   * nach 'target_position'. Dabei muss 'target_position' groesser als
   * 'source_position' sein. Diese Funktion wird ggf. von 'b_copy()'
   * aufgerufen.
   */

  extern void
b_clear( /* Buffer *buffer */ );
  /* Loescht den Buffer und setzt die Laenge auf 0.
   */

  extern long
b_read_buffer_from_file( /* Buffer *buffer, char *filename */ );
  /* Liest die Datei in den Buffer. Der bisherige Inhalt des Buffers wird
   * dabei geloescht. Die Funktion liefert die Anzahl der gelesenen Bytes
   * und -1 im Fehlerfall. Ob der Inhalt des Buffers im Fehlerfall erhalten
   * bleibt, haengt von der Art des aufgetretenen Fehlers ab.
   */

  extern long
b_write_buffer_to_file( /* Buffer *buffer, char *filename */ );
  /* Schreibt den Buffer in die angegebene Datei. Liefert die Anzahl der
   * geschriebenen Bytes und -1 im Fehlerfall.
   */

  extern long
b_copy_to_file( /* Buffer *buffer, char *filename,
                   long position, long count */ );
  /* Schreibt 'count' Bytes ab der Position 'position' in die Datei
   * 'filename'. Liefert die Anzahl der geschriebenen Bytes und -1 im
   * Fehlerfall.
   */

  extern long
b_paste_from_file( /* Buffer *buffer, char *filename, long position */ );
  /* Der Inhalt der Datei 'filename' wird an der Position 'position' in den
   * Buffer eingefuegt, d.h. es werden keine Daten ueberschrieben.
   */


  extern long
b_no_lines( /* Buffer *buffer */ );
  /* Liefert die Anzahl der Zeilen des Buffers.
   */

  extern long
b_goto_line( /* Buffer *buffer, long number */ );
  /* Liefert die Byteposition des Anfangs der Zeile 'number'. Falls
   * 'number' eine Zeilennummer groesser/gleich der Zeilenzahl des
   * Buffers ist, wird -1 geliefert.
   */

  extern long
b_get_linenumber( /* Buffer *buffer, long position */ );
  /* Liefert die Nummer der Zeile, in der sich die Position 'position'
   * befindet. Falls 'position' ausserhalb der Buffergrenzen liegt,
   * wird -1 geliefert.
   */

  extern long
b_line_start( /* Buffer *buffer, long position */ );
  /* Liefert die Position des Anfangs der Zeile, in der sich die Position
   * 'position' befindet. Falls 'position' ausserhalb der Buffergrenzen
   * liegt, wird -1 geliefert.
   * Ausnahme: Fuer negative Werte von 'position' wird immer 0 geliefert.
   */

  extern long
b_line_end( /* Buffer *buffer, long position */ );
  /* Liefert die Position des Zeilentrenners der Zeile, in der sich
   * die Position 'position' befindet. Falls 'position' ausserhalb der
   * Buffergrenzen liegt, wird -1 geliefert.
   */

  extern long
b_length_of_line( /* Buffer *buffer, long number */ );
  /* Liefert die Laenge der Zeile 'number' (incl. Zeilentrenner) und 0,
   * falls diese Zeile nicht existiert.
   * NOTE: Falls 'number' die letzte Zeile im Buffer ist und der Buffer
   *   *nicht* mit einem Zeilentrenner endet, tut die Funktion so, als
   *   waere der Zeilentrenner vorhanden.
   */

  extern long
b_length_of_text_block( /* Buffer *buffer, long number, long count */ );
  /* Liefert die Laenge des 'count' Zeilen langen Textblocks ab Zeile
   * 'number' (incl. der Zeilentrenner). Falls die Zeile 'number' nicht
   * existiert, wird 0 geliefert. Falls nach der Zeile 'number' keine
   * wieteren 'count - 1' Zeilen existieren, werden die nicht existierenden
   * Zeilen als Zeilen der Laenge 0 aufgefasst.
   * NOTE: Zeilen, die nur aus einem Zeilentrenner bestehen, sind Zeilen
   *   der Laenge 1.
   */

  extern long
b_read_line( /* Buffer *buffer, char *line, long number */ );
  /* Kopiert die Zeile 'number' nach 'line' und liefert die Laenge
   * der kopierten Zeile (incl. Endezeichen).
   * NOTE: Die Funktion ersetzt den Zeilerntrenner '\n' durch ein
   *   Stringende-Zeichen '\0', d.h. der Rueckgabewert der Funktion
   *   'b_length_of_line()' kann fuer den betreffenden 'malloc()'-Aufruf
   *   verwendet werden.
   */

  extern long
b_read_text_block( /* Buffer *buffer, char *target,
                      long number, long count */ );
  /* Kopiert ab Zeile 'number' (incl.) 'count' Zeilen nach 'target'.
   * Die Zeilentrenner werden mitkopiert, der letzte Zeilentrenner wird
   * durch ein '\0' ersetzt. Liefert die Anzahl der kopierten Bytes (analog
   * zu 'b_lenght_of_text_block()').
   */

  extern long
b_delete_line( /* Buffer *buffer, long number */ );
  /* Loescht die Zeile 'number' (incl. Zeilentrenner) aus dem Buffer.
   * Liefert die Anzahl der geloeschten Bytes und 0 bei einer ungueltigen
   * Zeilenangabe.
   */

  extern long
b_delete_text_block( /* Buffer *buffer, long number, long count */ );
  /* Loescht ab Zeile 'number' (incl.) 'count' Zeilen.
   * Liefert die Anzahl der geloeschten Bytes und 0 bei einer ungueltigen
   * Bereichsangabe.
   */

  extern long
b_clear_line( /* Buffer *buffer, long number */ );
  /* Wie 'b_delete_line()', nur dass der Zeilentrenner nicht mitgeloescht
   * wird. Geliefert wird die Anzahl der geloeschten Bytes + 1 und 0, falls
   * die Zeilenangabe ungueltig war.
   * NOTE: Falls eine Zeile nur den Zeilentrenner enthaelt, wird eine 1
   * geliefert.
   */

  extern long
b_insert_text_block( /* Buffer *buffer, char *source, long number */ );
  /* Der gegebene Textblock 'source' wird nach der Zeile 'number'
   * eingefuegt. Das Stringende-Zeichen '\0' von 'source' wird dabei in
   * einen Zeilentrenner umgewandelt. Falls 'number' eine negative Zahl
   * ist, wird der Textblock am Ende des Buffers angehaengt, falls 'number'
   * ausserhalb des Buffers liegt, wird  das als Fehler interpretiert.
   * Geliefert wird die Anzahl der eingefuegten Bytes und -1 im Fehlerfall. 
   */

/* In diesem Zusammenhang sind weitere Funktionen geplant, die den gelesenen
 * Text in einem 'Buffer' speichern.
 * b_copy_line, b_copy_text_block:
 *   Wie b_read_line, b_read_text_block.
 * b_paste_text_block:
 *   Wie b_insert_text_block, nur dass anstelle von 'source' ein 'Buffer'
 *   uebergeben wird.
 */

#endif

/* end of buffer.h */


/* VIM configuration: (do not delete this line)
 *
 * vim:aw:bk:ch=2:nodg:efm=%f\:%l\:%m:et:hid:icon:
 * vim:sw=2:sc:sm:si:textwidth=79:to:ul=1024:wh=12:wrap:wb:
 */
