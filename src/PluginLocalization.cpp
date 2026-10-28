#include "PluginLocalization.h"

#include <algorithm>
#include <cctype>

namespace MarkdownTablePlugin
{
namespace
{
#define UI_TEXT_WITH_ENGLISH_MESSAGES(menuName, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14) \
{ \
	menuName, \
	{ c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, NULL, NULL, NULL, NULL, NULL }, \
	L"Insert Markdown table", \
	L"Columns:", \
	L"Data rows:", \
	L"OK", \
	L"Cancel", \
	L"Use 1-50 columns and 0-200 data rows.", \
	L"Put the caret inside a Markdown pipe table first.", \
	L"Could not edit the Markdown table.", \
	L"Select CSV/TSV text or put the caret inside a CSV/TSV block first.", \
	L"Could not convert the selected CSV/TSV text.", \
	L"Power Auto fit and Notepad++ Word Wrap", \
	L"Notepad++ Word Wrap is on.\r\nPower Auto fit will edit Markdown table text to fit the window. Word Wrap only changes screen display.", \
	L"Do not show again" \
}

const UiText englishUiText =
{
	L"Markdown Table Editor",
	{
		L"Align table",
		L"Next cell",
		L"Previous cell",
		L"Insert row below",
		L"Delete row",
		L"Insert column right",
		L"Delete column",
		L"Move row up",
		L"Move row down",
		L"Move column left",
		L"Move column right",
		L"Sort rows ascending",
		L"Sort rows descending",
		L"Convert CSV/TSV to table",
		L"Insert table...",
		L"Narrow column",
		L"Widen column",
		NULL,
		NULL,
		NULL
	},
	L"Insert Markdown table",
	L"Columns:",
	L"Data rows:",
	L"OK",
	L"Cancel",
	L"Use 1-50 columns and 0-200 data rows.",
	L"Put the caret inside a Markdown pipe table first.",
	L"Could not edit the Markdown table.",
	L"Select CSV/TSV text or put the caret inside a CSV/TSV block first.",
	L"Could not convert the selected CSV/TSV text.",
	L"Power Auto fit and Notepad++ Word Wrap",
	L"Notepad++ Word Wrap is on.\r\nPower Auto fit will edit Markdown table text to fit the window. Word Wrap only changes screen display.",
	L"Do not show again"
};

const UiText chineseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u8868\u683C\u7F16\u8F91\u5668",
	L"\u5BF9\u9F50\u8868\u683C",
	L"\u4E0B\u4E00\u4E2A\u5355\u5143\u683C",
	L"\u4E0A\u4E00\u4E2A\u5355\u5143\u683C",
	L"\u5728\u4E0B\u65B9\u63D2\u5165\u884C",
	L"\u5220\u9664\u884C",
	L"\u5728\u53F3\u4FA7\u63D2\u5165\u5217",
	L"\u5220\u9664\u5217",
	L"\u4E0A\u79FB\u884C",
	L"\u4E0B\u79FB\u884C",
	L"\u5DE6\u79FB\u5217",
	L"\u53F3\u79FB\u5217",
	L"\u6309\u5347\u5E8F\u6392\u5E8F\u884C",
	L"\u6309\u964D\u5E8F\u6392\u5E8F\u884C",
	L"\u5C06 CSV/TSV \u8F6C\u4E3A\u8868\u683C",
	L"\u63D2\u5165\u65B0\u8868\u683C"
);

const UiText hindiUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u092A\u093E\u0926\u0915",
	L"\u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902",
	L"\u0905\u0917\u0932\u093E \u0938\u0947\u0932",
	L"\u092A\u093F\u091B\u0932\u093E \u0938\u0947\u0932",
	L"\u0928\u0940\u091A\u0947 \u092A\u0902\u0915\u094D\u0924\u093F \u0921\u093E\u0932\u0947\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F \u0939\u091F\u093E\u090F\u0902",
	L"\u0926\u093E\u0908\u0902 \u0913\u0930 \u0915\u0949\u0932\u092E \u0921\u093E\u0932\u0947\u0902",
	L"\u0915\u0949\u0932\u092E \u0939\u091F\u093E\u090F\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F \u090A\u092A\u0930 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F \u0928\u0940\u091A\u0947 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u0915\u0949\u0932\u092E \u092C\u093E\u090F\u0902 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u0915\u0949\u0932\u092E \u0926\u093E\u090F\u0902 \u0932\u0947 \u091C\u093E\u090F\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F\u092F\u093E\u0902 \u0906\u0930\u094B\u0939\u0940 \u0915\u094D\u0930\u092E \u092E\u0947\u0902 \u091B\u093E\u0902\u091F\u0947\u0902",
	L"\u092A\u0902\u0915\u094D\u0924\u093F\u092F\u093E\u0902 \u0905\u0935\u0930\u094B\u0939\u0940 \u0915\u094D\u0930\u092E \u092E\u0947\u0902 \u091B\u093E\u0902\u091F\u0947\u0902",
	L"CSV/TSV \u0915\u094B \u0924\u093E\u0932\u093F\u0915\u093E \u092E\u0947\u0902 \u092C\u0926\u0932\u0947\u0902",
	L"\u0928\u0908 \u0924\u093E\u0932\u093F\u0915\u093E \u0921\u093E\u0932\u0947\u0902"
);

const UiText spanishUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editor de tablas Markdown",
	L"Alinear tabla",
	L"Celda siguiente",
	L"Celda anterior",
	L"Insertar fila debajo",
	L"Eliminar fila",
	L"Insertar columna a la derecha",
	L"Eliminar columna",
	L"Mover fila arriba",
	L"Mover fila abajo",
	L"Mover columna a la izquierda",
	L"Mover columna a la derecha",
	L"Ordenar filas ascendente",
	L"Ordenar filas descendente",
	L"Convertir CSV/TSV en tabla",
	L"Insertar tabla nueva"
);

const UiText arabicUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"\u0645\u062D\u0631\u0631 \u062C\u062F\u0627\u0648\u0644 Markdown",
	L"\u0645\u062D\u0627\u0630\u0627\u0629 \u0627\u0644\u062C\u062F\u0648\u0644",
	L"\u0627\u0644\u062E\u0644\u064A\u0629 \u0627\u0644\u062A\u0627\u0644\u064A\u0629",
	L"\u0627\u0644\u062E\u0644\u064A\u0629 \u0627\u0644\u0633\u0627\u0628\u0642\u0629",
	L"\u0625\u062F\u0631\u0627\u062C \u0635\u0641 \u0628\u0627\u0644\u0623\u0633\u0641\u0644",
	L"\u062D\u0630\u0641 \u0627\u0644\u0635\u0641",
	L"\u0625\u062F\u0631\u0627\u062C \u0639\u0645\u0648\u062F \u0625\u0644\u0649 \u0627\u0644\u064A\u0645\u064A\u0646",
	L"\u062D\u0630\u0641 \u0627\u0644\u0639\u0645\u0648\u062F",
	L"\u0646\u0642\u0644 \u0627\u0644\u0635\u0641 \u0644\u0623\u0639\u0644\u0649",
	L"\u0646\u0642\u0644 \u0627\u0644\u0635\u0641 \u0644\u0623\u0633\u0641\u0644",
	L"\u0646\u0642\u0644 \u0627\u0644\u0639\u0645\u0648\u062F \u0625\u0644\u0649 \u0627\u0644\u064A\u0633\u0627\u0631",
	L"\u0646\u0642\u0644 \u0627\u0644\u0639\u0645\u0648\u062F \u0625\u0644\u0649 \u0627\u0644\u064A\u0645\u064A\u0646",
	L"\u0641\u0631\u0632 \u0627\u0644\u0635\u0641\u0648\u0641 \u062A\u0635\u0627\u0639\u062F\u064A\u0627",
	L"\u0641\u0631\u0632 \u0627\u0644\u0635\u0641\u0648\u0641 \u062A\u0646\u0627\u0632\u0644\u064A\u0627",
	L"\u062A\u062D\u0648\u064A\u0644 CSV/TSV \u0625\u0644\u0649 \u062C\u062F\u0648\u0644",
	L"\u0625\u062F\u0631\u0627\u062C \u062C\u062F\u0648\u0644 \u062C\u062F\u064A\u062F"
);

const UiText frenchUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editeur de tableaux Markdown",
	L"Aligner le tableau",
	L"Cellule suivante",
	L"Cellule precedente",
	L"Inserer une ligne dessous",
	L"Supprimer la ligne",
	L"Inserer une colonne a droite",
	L"Supprimer la colonne",
	L"Monter la ligne",
	L"Descendre la ligne",
	L"Deplacer la colonne a gauche",
	L"Deplacer la colonne a droite",
	L"Trier les lignes croissant",
	L"Trier les lignes decroissant",
	L"Convertir CSV/TSV en tableau",
	L"Inserer un nouveau tableau"
);

const UiText bengaliUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09AE\u09CD\u09AA\u09BE\u09A6\u0995",
	L"\u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8",
	L"\u09AA\u09B0\u09C7\u09B0 \u09B8\u09C7\u09B2",
	L"\u0986\u0997\u09C7\u09B0 \u09B8\u09C7\u09B2",
	L"\u09A8\u09BF\u099A\u09C7 \u09B8\u09BE\u09B0\u09BF \u09A2\u09CB\u0995\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u09AE\u09C1\u099B\u09C1\u09A8",
	L"\u09A1\u09BE\u09A8\u09C7 \u0995\u09B2\u09BE\u09AE \u09A2\u09CB\u0995\u09BE\u09A8",
	L"\u0995\u09B2\u09BE\u09AE \u09AE\u09C1\u099B\u09C1\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u0989\u09AA\u09B0\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u09A8\u09BF\u099A\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u0995\u09B2\u09BE\u09AE \u09AC\u09BE\u09AE\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u0995\u09B2\u09BE\u09AE \u09A1\u09BE\u09A8\u09C7 \u09B8\u09B0\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u098A\u09B0\u09CD\u09A7\u09CD\u09AC\u0995\u09CD\u09B0\u09AE\u09C7 \u09B8\u09BE\u099C\u09BE\u09A8",
	L"\u09B8\u09BE\u09B0\u09BF \u09A8\u09BF\u09AE\u09CD\u09A8\u0995\u09CD\u09B0\u09AE\u09C7 \u09B8\u09BE\u099C\u09BE\u09A8",
	L"CSV/TSV \u099F\u09C7\u09AC\u09BF\u09B2\u09C7 \u09B0\u09C2\u09AA\u09BE\u09A8\u09CD\u09A4\u09B0 \u0995\u09B0\u09C1\u09A8",
	L"\u09A8\u09A4\u09C1\u09A8 \u099F\u09C7\u09AC\u09BF\u09B2 \u09A2\u09CB\u0995\u09BE\u09A8"
);

const UiText portugueseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editor de tabelas Markdown",
	L"Alinhar tabela",
	L"Proxima celula",
	L"Celula anterior",
	L"Inserir linha abaixo",
	L"Excluir linha",
	L"Inserir coluna a direita",
	L"Excluir coluna",
	L"Mover linha para cima",
	L"Mover linha para baixo",
	L"Mover coluna para a esquerda",
	L"Mover coluna para a direita",
	L"Ordenar linhas crescente",
	L"Ordenar linhas decrescente",
	L"Converter CSV/TSV em tabela",
	L"Inserir nova tabela"
);

const UiText indonesianUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Editor Tabel Markdown",
	L"Ratakan tabel",
	L"Sel berikutnya",
	L"Sel sebelumnya",
	L"Sisipkan baris di bawah",
	L"Hapus baris",
	L"Sisipkan kolom kanan",
	L"Hapus kolom",
	L"Pindahkan baris ke atas",
	L"Pindahkan baris ke bawah",
	L"Pindahkan kolom ke kiri",
	L"Pindahkan kolom ke kanan",
	L"Urutkan baris naik",
	L"Urutkan baris turun",
	L"Konversi CSV/TSV ke tabel",
	L"Sisipkan tabel baru"
);

const UiText urduUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u062C\u062F\u0648\u0644 \u0627\u06CC\u0688\u06CC\u0679\u0631",
	L"\u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA",
	L"\u0627\u06AF\u0644\u0627 \u062E\u0627\u0646\u06C1",
	L"\u067E\u0686\u06BE\u0644\u0627 \u062E\u0627\u0646\u06C1",
	L"\u0646\u06CC\u0686\u06D2 \u0642\u0637\u0627\u0631 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631 \u062D\u0630\u0641 \u06A9\u0631\u06CC\u06BA",
	L"\u062F\u0627\u0626\u06CC\u06BA \u06A9\u0627\u0644\u0645 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u06A9\u0627\u0644\u0645 \u062D\u0630\u0641 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631 \u0627\u0648\u067E\u0631 \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631 \u0646\u06CC\u0686\u06D2 \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u06A9\u0627\u0644\u0645 \u0628\u0627\u0626\u06CC\u06BA \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u06A9\u0627\u0644\u0645 \u062F\u0627\u0626\u06CC\u06BA \u0645\u0646\u062A\u0642\u0644 \u06A9\u0631\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631\u06CC\u06BA \u0635\u0639\u0648\u062F\u06CC \u062A\u0631\u062A\u06CC\u0628 \u062F\u06CC\u06BA",
	L"\u0642\u0637\u0627\u0631\u06CC\u06BA \u0646\u0632\u0648\u0644\u06CC \u062A\u0631\u062A\u06CC\u0628 \u062F\u06CC\u06BA",
	L"CSV/TSV \u06A9\u0648 \u062C\u062F\u0648\u0644 \u0628\u0646\u0627\u0626\u06CC\u06BA",
	L"\u0646\u06CC\u0627 \u062C\u062F\u0648\u0644 \u062F\u0627\u062E\u0644 \u06A9\u0631\u06CC\u06BA"
);

const UiText germanUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown-Tabelleneditor",
	L"Tabelle ausrichten",
	L"Naechste Zelle",
	L"Vorherige Zelle",
	L"Zeile darunter einfuegen",
	L"Zeile loeschen",
	L"Spalte rechts einfuegen",
	L"Spalte loeschen",
	L"Zeile nach oben",
	L"Zeile nach unten",
	L"Spalte nach links",
	L"Spalte nach rechts",
	L"Zeilen aufsteigend sortieren",
	L"Zeilen absteigend sortieren",
	L"CSV/TSV in Tabelle umwandeln",
	L"Neue Tabelle einfuegen"
);

const UiText russianUiText =
{
	L"\u0420\u0435\u0434\u0430\u043A\u0442\u043E\u0440 Markdown-\u0442\u0430\u0431\u043B\u0438\u0446",
	{
		L"\u0412\u044B\u0440\u043E\u0432\u043D\u044F\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443",
		L"\u0421\u043B\u0435\u0434\u0443\u044E\u0449\u0430\u044F \u044F\u0447\u0435\u0439\u043A\u0430",
		L"\u041F\u0440\u0435\u0434\u044B\u0434\u0443\u0449\u0430\u044F \u044F\u0447\u0435\u0439\u043A\u0430",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u043D\u0438\u0436\u0435",
		L"\u0423\u0434\u0430\u043B\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0441\u043F\u0440\u0430\u0432\u0430",
		L"\u0423\u0434\u0430\u043B\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u0432\u0432\u0435\u0440\u0445",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0443 \u0432\u043D\u0438\u0437",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0432\u043B\u0435\u0432\u043E",
		L"\u041F\u0435\u0440\u0435\u043C\u0435\u0441\u0442\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446 \u0432\u043F\u0440\u0430\u0432\u043E",
		L"\u0421\u043E\u0440\u0442\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0432\u043E\u0437\u0440\u0430\u0441\u0442\u0430\u043D\u0438\u044E",
		L"\u0421\u043E\u0440\u0442\u0438\u0440\u043E\u0432\u0430\u0442\u044C \u0441\u0442\u0440\u043E\u043A\u0438 \u043F\u043E \u0443\u0431\u044B\u0432\u0430\u043D\u0438\u044E",
		L"\u041F\u0440\u0435\u043E\u0431\u0440\u0430\u0437\u043E\u0432\u0430\u0442\u044C CSV/TSV \u0432 \u0442\u0430\u0431\u043B\u0438\u0446\u0443",
		L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443...",
		NULL,
		NULL,
		NULL
	},
	L"\u0412\u0441\u0442\u0430\u0432\u0438\u0442\u044C Markdown-\u0442\u0430\u0431\u043B\u0438\u0446\u0443",
	L"\u0421\u0442\u043E\u043B\u0431\u0446\u044B:",
	L"\u0421\u0442\u0440\u043E\u043A\u0438 \u0434\u0430\u043D\u043D\u044B\u0445:",
	L"\u041E\u041A",
	L"\u041E\u0442\u043C\u0435\u043D\u0430",
	L"\u0414\u043E\u043F\u0443\u0441\u0442\u0438\u043C\u043E: 1-50 \u0441\u0442\u043E\u043B\u0431\u0446\u043E\u0432 \u0438 0-200 \u0441\u0442\u0440\u043E\u043A \u0434\u0430\u043D\u043D\u044B\u0445.",
	L"\u041F\u043E\u043C\u0435\u0441\u0442\u0438\u0442\u0435 \u043A\u0443\u0440\u0441\u043E\u0440 \u0432\u043D\u0443\u0442\u0440\u044C Markdown-\u0442\u0430\u0431\u043B\u0438\u0446\u044B \u0441 \u0432\u0435\u0440\u0442\u0438\u043A\u0430\u043B\u044C\u043D\u044B\u043C\u0438 \u0447\u0435\u0440\u0442\u0430\u043C\u0438.",
	L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u0438\u0437\u043C\u0435\u043D\u0438\u0442\u044C Markdown-\u0442\u0430\u0431\u043B\u0438\u0446\u0443.",
	L"\u0412\u044B\u0434\u0435\u043B\u0438\u0442\u0435 CSV/TSV-\u0442\u0435\u043A\u0441\u0442 \u0438\u043B\u0438 \u043F\u043E\u043C\u0435\u0441\u0442\u0438\u0442\u0435 \u043A\u0443\u0440\u0441\u043E\u0440 \u0432\u043D\u0443\u0442\u0440\u044C CSV/TSV-\u0431\u043B\u043E\u043A\u0430.",
	L"\u041D\u0435 \u0443\u0434\u0430\u043B\u043E\u0441\u044C \u043F\u0440\u0435\u043E\u0431\u0440\u0430\u0437\u043E\u0432\u0430\u0442\u044C \u0432\u044B\u0431\u0440\u0430\u043D\u043D\u044B\u0439 CSV/TSV-\u0442\u0435\u043A\u0441\u0442.",
	L"Power \u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0438 \u043F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A",
	L"\u041F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A Notepad++ \u0432\u043A\u043B\u044E\u0447\u0451\u043D.\r\nPower \u0430\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0431\u0443\u0434\u0435\u0442 \u043C\u0435\u043D\u044F\u0442\u044C Markdown-\u0442\u0435\u043A\u0441\u0442 \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u0448\u0438\u0440\u0438\u043D\u0443 \u043E\u043A\u043D\u0430. \u041F\u0435\u0440\u0435\u043D\u043E\u0441 \u0441\u0442\u0440\u043E\u043A Notepad++ \u0432\u043B\u0438\u044F\u0435\u0442 \u0442\u043E\u043B\u044C\u043A\u043E \u043D\u0430 \u043E\u0442\u043E\u0431\u0440\u0430\u0436\u0435\u043D\u0438\u0435.",
	L"\u0411\u043E\u043B\u044C\u0448\u0435 \u043D\u0435 \u043F\u043E\u043A\u0430\u0437\u044B\u0432\u0430\u0442\u044C"
};

const UiText japaneseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u30C6\u30FC\u30D6\u30EB\u30A8\u30C7\u30A3\u30BF\u30FC",
	L"\u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217",
	L"\u6B21\u306E\u30BB\u30EB",
	L"\u524D\u306E\u30BB\u30EB",
	L"\u4E0B\u306B\u884C\u3092\u633F\u5165",
	L"\u884C\u3092\u524A\u9664",
	L"\u53F3\u306B\u5217\u3092\u633F\u5165",
	L"\u5217\u3092\u524A\u9664",
	L"\u884C\u3092\u4E0A\u3078\u79FB\u52D5",
	L"\u884C\u3092\u4E0B\u3078\u79FB\u52D5",
	L"\u5217\u3092\u5DE6\u3078\u79FB\u52D5",
	L"\u5217\u3092\u53F3\u3078\u79FB\u52D5",
	L"\u884C\u3092\u6607\u9806\u3067\u4E26\u3079\u66FF\u3048",
	L"\u884C\u3092\u964D\u9806\u3067\u4E26\u3079\u66FF\u3048",
	L"CSV/TSV \u3092\u30C6\u30FC\u30D6\u30EB\u306B\u5909\u63DB",
	L"\u65B0\u3057\u3044\u30C6\u30FC\u30D6\u30EB\u3092\u633F\u5165"
);

const UiText nigerianPidginUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown Table Editor",
	L"Arrange table",
	L"Next cell",
	L"Previous cell",
	L"Put row below",
	L"Delete row",
	L"Put column for right",
	L"Delete column",
	L"Move row up",
	L"Move row down",
	L"Move column left",
	L"Move column right",
	L"Sort rows small to big",
	L"Sort rows big to small",
	L"Change CSV/TSV to table",
	L"Put new table"
);

const UiText marathiUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0924\u0915\u094D\u0924\u093E \u0938\u0902\u092A\u093E\u0926\u0915",
	L"\u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E",
	L"\u092A\u0941\u0922\u0940\u0932 \u0938\u0947\u0932",
	L"\u092E\u093E\u0917\u0940\u0932 \u0938\u0947\u0932",
	L"\u0916\u093E\u0932\u0940 \u0913\u0933 \u0918\u093E\u0932\u093E",
	L"\u0913\u0933 \u0939\u091F\u0935\u093E",
	L"\u0909\u091C\u0935\u0940\u0915\u0921\u0947 \u0938\u094D\u0924\u0902\u092D \u0918\u093E\u0932\u093E",
	L"\u0938\u094D\u0924\u0902\u092D \u0939\u091F\u0935\u093E",
	L"\u0913\u0933 \u0935\u0930 \u0939\u0932\u0935\u093E",
	L"\u0913\u0933 \u0916\u093E\u0932\u0940 \u0939\u0932\u0935\u093E",
	L"\u0938\u094D\u0924\u0902\u092D \u0921\u093E\u0935\u0940\u0915\u0921\u0947 \u0939\u0932\u0935\u093E",
	L"\u0938\u094D\u0924\u0902\u092D \u0909\u091C\u0935\u0940\u0915\u0921\u0947 \u0939\u0932\u0935\u093E",
	L"\u0913\u0933\u0940 \u091A\u0922\u0924\u094D\u092F\u093E \u0915\u094D\u0930\u092E\u093E\u0928\u0947 \u0932\u093E\u0935\u093E",
	L"\u0913\u0933\u0940 \u0909\u0924\u0930\u0924\u094D\u092F\u093E \u0915\u094D\u0930\u092E\u093E\u0928\u0947 \u0932\u093E\u0935\u093E",
	L"CSV/TSV \u0924\u0915\u094D\u0924\u094D\u092F\u093E\u0924 \u092C\u0926\u0932\u093E",
	L"\u0928\u0935\u093E \u0924\u0915\u094D\u0924\u093E \u0918\u093E\u0932\u093E"
);

const UiText teluguUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15 \u0C0E\u0C21\u0C3F\u0C1F\u0C30\u0C4D",
	L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41",
	L"\u0C24\u0C26\u0C41\u0C2A\u0C30\u0C3F \u0C38\u0C46\u0C32\u0C4D",
	L"\u0C2E\u0C41\u0C28\u0C41\u0C2A\u0C1F\u0C3F \u0C38\u0C46\u0C32\u0C4D",
	L"\u0C15\u0C4D\u0C30\u0C3F\u0C02\u0C26 \u0C35\u0C30\u0C41\u0C38 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C24\u0C4A\u0C32\u0C17\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C15\u0C41\u0C21\u0C3F\u0C35\u0C48\u0C2A\u0C41 \u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C24\u0C4A\u0C32\u0C17\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C2A\u0C48\u0C15\u0C3F \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C15\u0C4D\u0C30\u0C3F\u0C02\u0C26\u0C3F\u0C15\u0C3F \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C0E\u0C21\u0C2E\u0C15\u0C41 \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C28\u0C3F\u0C32\u0C41\u0C35\u0C41 \u0C35\u0C30\u0C41\u0C38\u0C28\u0C41 \u0C15\u0C41\u0C21\u0C3F\u0C15\u0C3F \u0C24\u0C30\u0C32\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C32\u0C28\u0C41 \u0C06\u0C30\u0C4B\u0C39\u0C23\u0C02\u0C17\u0C3E \u0C15\u0C4D\u0C30\u0C2E\u0C2C\u0C26\u0C4D\u0C27\u0C40\u0C15\u0C30\u0C3F\u0C02\u0C1A\u0C41",
	L"\u0C35\u0C30\u0C41\u0C38\u0C32\u0C28\u0C41 \u0C05\u0C35\u0C30\u0C4B\u0C39\u0C23\u0C02\u0C17\u0C3E \u0C15\u0C4D\u0C30\u0C2E\u0C2C\u0C26\u0C4D\u0C27\u0C40\u0C15\u0C30\u0C3F\u0C02\u0C1A\u0C41",
	L"CSV/TSV \u0C28\u0C41 \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C17\u0C3E \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C41",
	L"\u0C15\u0C4A\u0C24\u0C4D\u0C24 \u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C1A\u0C4A\u0C2A\u0C4D\u0C2A\u0C3F\u0C02\u0C1A\u0C41"
);

const UiText turkishUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown Tablo D\u00FCzenleyici",
	L"Tabloyu hizala",
	L"Sonraki h\u00FCcre",
	L"\u00D6nceki h\u00FCcre",
	L"Alta sat\u0131r ekle",
	L"Sat\u0131r\u0131 sil",
	L"Sa\u011Fa s\u00FCtun ekle",
	L"S\u00FCtunu sil",
	L"Sat\u0131r\u0131 yukar\u0131 ta\u015F\u0131",
	L"Sat\u0131r\u0131 a\u015Fa\u011F\u0131 ta\u015F\u0131",
	L"S\u00FCtunu sola ta\u015F\u0131",
	L"S\u00FCtunu sa\u011Fa ta\u015F\u0131",
	L"Sat\u0131rlar\u0131 artan s\u0131rala",
	L"Sat\u0131rlar\u0131 azalan s\u0131rala",
	L"CSV/TSV'yi tabloya d\u00F6n\u00FC\u015Ft\u00FCr",
	L"Yeni tablo ekle"
);

const UiText tamilUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0BA4\u0BBF\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BBF",
	L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0B85\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4 \u0B9A\u0BC6\u0BB2\u0BCD",
	L"\u0BAE\u0BC1\u0BA8\u0BCD\u0BA4\u0BC8\u0BAF \u0B9A\u0BC6\u0BB2\u0BCD",
	L"\u0B95\u0BC0\u0BB4\u0BC7 \u0BB5\u0BB0\u0BBF\u0B9A\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0BA8\u0BC0\u0B95\u0BCD\u0B95\u0BC1",
	L"\u0BB5\u0BB2\u0BAA\u0BCD\u0BAA\u0BC1\u0BB1\u0BAE\u0BCD \u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1",
	L"\u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8 \u0BA8\u0BC0\u0B95\u0BCD\u0B95\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0BAE\u0BC7\u0BB2\u0BC7 \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0B95\u0BC0\u0BB4\u0BC7 \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0B87\u0B9F\u0BAA\u0BCD\u0BAA\u0BC1\u0BB1\u0BAE\u0BCD \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BA8\u0BC6\u0B9F\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BC8 \u0BB5\u0BB2\u0BAA\u0BCD\u0BAA\u0BC1\u0BB1\u0BAE\u0BCD \u0BA8\u0B95\u0BB0\u0BCD\u0BA4\u0BCD\u0BA4\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0B95\u0BB3\u0BC8 \u0B8F\u0BB1\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BBF\u0BB2\u0BCD \u0B85\u0B9F\u0BC1\u0B95\u0BCD\u0B95\u0BC1",
	L"\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0B95\u0BB3\u0BC8 \u0B87\u0BB1\u0B99\u0BCD\u0B95\u0BC1\u0BB5\u0BB0\u0BBF\u0B9A\u0BC8\u0BAF\u0BBF\u0BB2\u0BCD \u0B85\u0B9F\u0BC1\u0B95\u0BCD\u0B95\u0BC1",
	L"CSV/TSV \u0B90 \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BBE\u0B95 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BC1",
	L"\u0BAA\u0BC1\u0BA4\u0BBF\u0BAF \u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B9A\u0BC6\u0BB0\u0BC1\u0B95\u0BC1"
);

const UiText yueChineseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Markdown \u8868\u683C\u7DE8\u8F2F\u5668",
	L"\u5C0D\u9F4A\u8868\u683C",
	L"\u4E0B\u4E00\u500B\u5132\u5B58\u683C",
	L"\u4E0A\u4E00\u500B\u5132\u5B58\u683C",
	L"\u55BA\u4E0B\u9762\u63D2\u5165\u5217",
	L"\u522A\u9664\u5217",
	L"\u55BA\u53F3\u908A\u63D2\u5165\u6B04",
	L"\u522A\u9664\u6B04",
	L"\u4E0A\u79FB\u5217",
	L"\u4E0B\u79FB\u5217",
	L"\u5DE6\u79FB\u6B04",
	L"\u53F3\u79FB\u6B04",
	L"\u905E\u589E\u6392\u5E8F\u5217",
	L"\u905E\u6E1B\u6392\u5E8F\u5217",
	L"\u5C07 CSV/TSV \u8F49\u6210\u8868\u683C",
	L"\u63D2\u5165\u65B0\u8868\u683C"
);

const UiText vietnameseUiText = UI_TEXT_WITH_ENGLISH_MESSAGES(
	L"Tr\u00ECnh s\u1EEDa b\u1EA3ng Markdown",
	L"C\u0103n ch\u1EC9nh b\u1EA3ng",
	L"\u00D4 ti\u1EBFp theo",
	L"\u00D4 tr\u01B0\u1EDBc",
	L"Ch\u00E8n h\u00E0ng b\u00EAn d\u01B0\u1EDBi",
	L"X\u00F3a h\u00E0ng",
	L"Ch\u00E8n c\u1ED9t b\u00EAn ph\u1EA3i",
	L"X\u00F3a c\u1ED9t",
	L"Di chuy\u1EC3n h\u00E0ng l\u00EAn",
	L"Di chuy\u1EC3n h\u00E0ng xu\u1ED1ng",
	L"Di chuy\u1EC3n c\u1ED9t sang tr\u00E1i",
	L"Di chuy\u1EC3n c\u1ED9t sang ph\u1EA3i",
	L"S\u1EAFp x\u1EBFp h\u00E0ng t\u0103ng d\u1EA7n",
	L"S\u1EAFp x\u1EBFp h\u00E0ng gi\u1EA3m d\u1EA7n",
	L"Chuy\u1EC3n CSV/TSV th\u00E0nh b\u1EA3ng",
	L"Ch\u00E8n b\u1EA3ng m\u1EDBi"
);

std::size_t legacyCommandIndex(std::size_t index)
{
	switch (index)
	{
	case nextCellCommandIndex: return 1;
	case previousCellCommandIndex: return 2;
	case insertRowCommandIndex: return 3;
	case deleteRowCommandIndex: return 4;
	case insertColumnCommandIndex: return 5;
	case deleteColumnCommandIndex: return 6;
	case narrowColumnCommandIndex: return 15;
	case widenColumnCommandIndex: return 16;
	case moveRowUpCommandIndex: return 7;
	case moveRowDownCommandIndex: return 8;
	case moveColumnLeftCommandIndex: return 9;
	case moveColumnRightCommandIndex: return 10;
	case sortRowsAscendingCommandIndex: return 11;
	case sortRowsDescendingCommandIndex: return 12;
	case convertCsvTsvCommandIndex: return 13;
	case insertTableCommandIndex: return 14;
	default: return static_cast<std::size_t>(-1);
	}
}

const wchar_t *commandShortcutText(std::size_t index)
{
	switch (index)
	{
	case alignCommandIndex: return L"Ctrl+Alt+Shift+1";
	case autoAlignTableCommandIndex: return L"Ctrl+Alt+Shift+A";
	case wrapLongCellsCommandIndex: return L"Ctrl+Alt+Shift+W";
	case autoFitTableCommandIndex: return L"Ctrl+Alt+Shift+F";
	case nextCellCommandIndex: return L"Ctrl+Alt+Shift+2";
	case previousCellCommandIndex: return L"Ctrl+Alt+Shift+3";
	case insertRowCommandIndex: return L"Ctrl+Alt+Shift+4";
	case deleteRowCommandIndex: return L"Ctrl+Alt+Shift+5";
	case insertColumnCommandIndex: return L"Ctrl+Alt+Shift+6";
	case deleteColumnCommandIndex: return L"Ctrl+Alt+Shift+7";
	case narrowColumnCommandIndex: return L"Ctrl+Alt+Shift+,";
	case widenColumnCommandIndex: return L"Ctrl+Alt+Shift+.";
	case moveRowUpCommandIndex: return L"Ctrl+Alt+Shift+8";
	case moveRowDownCommandIndex: return L"Ctrl+Alt+Shift+9";
	case moveColumnLeftCommandIndex: return L"Ctrl+Alt+Shift+[";
	case moveColumnRightCommandIndex: return L"Ctrl+Alt+Shift+]";
	case sortRowsAscendingCommandIndex: return L"Ctrl+Alt+Shift+=";
	case sortRowsDescendingCommandIndex: return L"Ctrl+Alt+Shift+-";
	case convertCsvTsvCommandIndex: return L"Ctrl+Alt+Shift+0";
	case insertTableCommandIndex: return L"Ctrl+Alt+Shift+\\";
	default: return NULL;
	}
}

std::string toLowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return value;
}

bool containsMarker(const std::string &value, const char *marker)
{
	return value.find(marker) != std::string::npos;
}

bool matchesLanguageCode(const std::string &value, const char *code)
{
	const std::string exact = std::string(code) + ".xml";
	const std::string hyphenPrefix = std::string(code) + "-";
	const std::string underscorePrefix = std::string(code) + "_";
	return value == exact || value.find(hyphenPrefix) == 0 || value.find(underscorePrefix) == 0;
}

}

UiLanguage languageFromNativeLangFileName(const std::string &nativeLangFileName)
{
	const std::string lower = toLowerAscii(nativeLangFileName);
	if (containsMarker(lower, "chinesetraditional") || containsMarker(lower, "chinese_traditional") ||
		containsMarker(lower, "traditionalchinese") || containsMarker(lower, "cantonese") ||
		matchesLanguageCode(lower, "yue") || matchesLanguageCode(lower, "zh-hk") ||
		matchesLanguageCode(lower, "zh_hk") || matchesLanguageCode(lower, "zh-tw") ||
		matchesLanguageCode(lower, "zh_tw"))
		return UiLanguage::YueChinese;
	if (containsMarker(lower, "chinesesimplified") || containsMarker(lower, "chinese_simplified") ||
		containsMarker(lower, "simplifiedchinese") || containsMarker(lower, "mandarin") ||
		matchesLanguageCode(lower, "zh-cn") || matchesLanguageCode(lower, "zh_cn") ||
		matchesLanguageCode(lower, "zh"))
		return UiLanguage::MandarinChinese;
	if (containsMarker(lower, "hindi") || matchesLanguageCode(lower, "hi"))
		return UiLanguage::Hindi;
	if (containsMarker(lower, "spanish") || containsMarker(lower, "espanol") || matchesLanguageCode(lower, "es"))
		return UiLanguage::Spanish;
	if (containsMarker(lower, "arabic") || matchesLanguageCode(lower, "ar"))
		return UiLanguage::Arabic;
	if (containsMarker(lower, "french") || containsMarker(lower, "francais") || matchesLanguageCode(lower, "fr"))
		return UiLanguage::French;
	if (containsMarker(lower, "bengali") || containsMarker(lower, "bangla") || matchesLanguageCode(lower, "bn"))
		return UiLanguage::Bengali;
	if (containsMarker(lower, "portuguese") || containsMarker(lower, "portugues") || matchesLanguageCode(lower, "pt"))
		return UiLanguage::Portuguese;
	if (containsMarker(lower, "russian") || matchesLanguageCode(lower, "ru"))
		return UiLanguage::Russian;
	if (containsMarker(lower, "indonesian") || containsMarker(lower, "bahasaindonesia") || matchesLanguageCode(lower, "id"))
		return UiLanguage::Indonesian;
	if (containsMarker(lower, "urdu") || matchesLanguageCode(lower, "ur"))
		return UiLanguage::Urdu;
	if (containsMarker(lower, "german") || containsMarker(lower, "deutsch") || matchesLanguageCode(lower, "de"))
		return UiLanguage::German;
	if (containsMarker(lower, "japanese") || matchesLanguageCode(lower, "ja"))
		return UiLanguage::Japanese;
	if (containsMarker(lower, "pidgin") || containsMarker(lower, "nigerian") || matchesLanguageCode(lower, "pcm"))
		return UiLanguage::NigerianPidgin;
	if (containsMarker(lower, "marathi") || matchesLanguageCode(lower, "mr"))
		return UiLanguage::Marathi;
	if (containsMarker(lower, "telugu") || matchesLanguageCode(lower, "te"))
		return UiLanguage::Telugu;
	if (containsMarker(lower, "turkish") || containsMarker(lower, "turkce") || matchesLanguageCode(lower, "tr"))
		return UiLanguage::Turkish;
	if (containsMarker(lower, "tamil") || matchesLanguageCode(lower, "ta"))
		return UiLanguage::Tamil;
	if (containsMarker(lower, "vietnamese") || containsMarker(lower, "tiengviet") || matchesLanguageCode(lower, "vi"))
		return UiLanguage::Vietnamese;
	return UiLanguage::English;
}

const UiText &PluginLocalization::uiText() const
{
	switch (language_)
	{
	case UiLanguage::MandarinChinese:
		return chineseUiText;
	case UiLanguage::Hindi:
		return hindiUiText;
	case UiLanguage::Spanish:
		return spanishUiText;
	case UiLanguage::Arabic:
		return arabicUiText;
	case UiLanguage::French:
		return frenchUiText;
	case UiLanguage::Bengali:
		return bengaliUiText;
	case UiLanguage::Portuguese:
		return portugueseUiText;
	case UiLanguage::Russian:
		return russianUiText;
	case UiLanguage::Indonesian:
		return indonesianUiText;
	case UiLanguage::Urdu:
		return urduUiText;
	case UiLanguage::German:
		return germanUiText;
	case UiLanguage::Japanese:
		return japaneseUiText;
	case UiLanguage::NigerianPidgin:
		return nigerianPidginUiText;
	case UiLanguage::Marathi:
		return marathiUiText;
	case UiLanguage::Telugu:
		return teluguUiText;
	case UiLanguage::Turkish:
		return turkishUiText;
	case UiLanguage::Tamil:
		return tamilUiText;
	case UiLanguage::YueChinese:
		return yueChineseUiText;
	case UiLanguage::Vietnamese:
		return vietnameseUiText;
	default:
		return englishUiText;
	}
}

const wchar_t *PluginLocalization::localizedSpecialCommandText(std::size_t index) const
{
	switch (language_)
	{
	case UiLanguage::MandarinChinese:
		switch (index)
		{
		case alignCommandIndex: return L"\u5BF9\u9F50\u8868\u683C\uFF08\u4E0D\u6539\u53D8\u5BBD\u5EA6\uFF09";
		case autoAlignTableCommandIndex: return L"\u7F16\u8F91\u540E\u81EA\u52A8\u5BF9\u9F50\uFF08\u4E0D\u6539\u53D8\u5BBD\u5EA6\uFF09";
		case wrapLongCellsCommandIndex: return L"\u5C06\u8868\u683C\u5BBD\u5EA6\u9002\u5E94\u7A97\u53E3";
		case autoFitTableCommandIndex: return L"\u81EA\u52A8\u5C06\u8868\u683C\u5BBD\u5EA6\u9002\u5E94\u7A97\u53E3";
		default: return NULL;
		}
	case UiLanguage::Hindi:
		switch (index)
		{
		case alignCommandIndex: return L"\u0924\u093E\u0932\u093F\u0915\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902 (\u091A\u094C\u0921\u093C\u093E\u0908 \u0928 \u092C\u0926\u0932\u0947\u0902)";
		case autoAlignTableCommandIndex: return L"\u0938\u0902\u092A\u093E\u0926\u0928 \u0915\u0947 \u092C\u093E\u0926 \u0938\u094D\u0935\u0924\u0903 \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u0947\u0902 (\u091A\u094C\u0921\u093C\u093E\u0908 \u0928 \u092C\u0926\u0932\u0947\u0902)";
		case wrapLongCellsCommandIndex: return L"\u0924\u093E\u0932\u093F\u0915\u093E \u0915\u0940 \u091A\u094C\u0921\u093C\u093E\u0908 \u0915\u094B \u0935\u093F\u0902\u0921\u094B \u092E\u0947\u0902 \u092B\u093F\u091F \u0915\u0930\u0947\u0902";
		case autoFitTableCommandIndex: return L"\u0924\u093E\u0932\u093F\u0915\u093E \u0915\u0940 \u091A\u094C\u0921\u093C\u093E\u0908 \u0915\u094B \u0935\u093F\u0902\u0921\u094B \u092E\u0947\u0902 \u0938\u094D\u0935\u0924\u0903 \u092B\u093F\u091F \u0915\u0930\u0947\u0902";
		default: return NULL;
		}
	case UiLanguage::Spanish:
		switch (index)
		{
		case alignCommandIndex: return L"Alinear tabla (sin cambiar ancho)";
		case autoAlignTableCommandIndex: return L"Alinear automaticamente tras editar (sin cambiar ancho)";
		case wrapLongCellsCommandIndex: return L"Ajustar ancho de tabla a ventana";
		case autoFitTableCommandIndex: return L"Ajustar automaticamente ancho de tabla a ventana";
		default: return NULL;
		}
	case UiLanguage::Arabic:
		switch (index)
		{
		case alignCommandIndex: return L"\u0645\u062D\u0627\u0630\u0627\u0629 \u0627\u0644\u062C\u062F\u0648\u0644 (\u0628\u062F\u0648\u0646 \u062A\u063A\u064A\u064A\u0631 \u0627\u0644\u0639\u0631\u0636)";
		case autoAlignTableCommandIndex: return L"\u0645\u062D\u0627\u0630\u0627\u0629 \u062A\u0644\u0642\u0627\u0626\u064A\u0629 \u0628\u0639\u062F \u0627\u0644\u062A\u062D\u0631\u064A\u0631 (\u0628\u062F\u0648\u0646 \u062A\u063A\u064A\u064A\u0631 \u0627\u0644\u0639\u0631\u0636)";
		case wrapLongCellsCommandIndex: return L"\u0645\u0644\u0627\u0621\u0645\u0629 \u0639\u0631\u0636 \u0627\u0644\u062C\u062F\u0648\u0644 \u0644\u0644\u0646\u0627\u0641\u0630\u0629";
		case autoFitTableCommandIndex: return L"\u0645\u0644\u0627\u0621\u0645\u0629 \u062A\u0644\u0642\u0627\u0626\u064A\u0629 \u0644\u0639\u0631\u0636 \u0627\u0644\u062C\u062F\u0648\u0644 \u0644\u0644\u0646\u0627\u0641\u0630\u0629";
		default: return NULL;
		}
	case UiLanguage::French:
		switch (index)
		{
		case alignCommandIndex: return L"Aligner le tableau (sans changer la largeur)";
		case autoAlignTableCommandIndex: return L"Alignement auto apres modification (sans changer la largeur)";
		case wrapLongCellsCommandIndex: return L"Ajuster la largeur du tableau a la fenetre";
		case autoFitTableCommandIndex: return L"Ajustement auto de la largeur du tableau a la fenetre";
		default: return NULL;
		}
	case UiLanguage::Bengali:
		switch (index)
		{
		case alignCommandIndex: return L"\u099F\u09C7\u09AC\u09BF\u09B2 \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7 \u0995\u09B0\u09C1\u09A8 (\u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u09A8\u09BE \u09AC\u09A6\u09B2\u09C7)";
		case autoAlignTableCommandIndex: return L"\u09B8\u09AE\u09CD\u09AA\u09BE\u09A6\u09A8\u09BE\u09B0 \u09AA\u09B0\u09C7 \u09B8\u09CD\u09AC\u09AF\u09BC\u0982\u0995\u09CD\u09B0\u09BF\u09AF\u09BC \u09B8\u09BE\u09B0\u09BF\u09AC\u09A6\u09CD\u09A7\u0995\u09B0\u09A3 (\u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u09A8\u09BE \u09AC\u09A6\u09B2\u09C7)";
		case wrapLongCellsCommandIndex: return L"\u099F\u09C7\u09AC\u09BF\u09B2\u09C7\u09B0 \u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u0989\u0987\u09A8\u09CD\u09A1\u09CB\u09B0 \u09B8\u09BE\u09A5\u09C7 \u09AE\u09BE\u09A8\u09BE\u09A8\u09B8\u0987 \u0995\u09B0\u09C1\u09A8";
		case autoFitTableCommandIndex: return L"\u099F\u09C7\u09AC\u09BF\u09B2\u09C7\u09B0 \u09AA\u09CD\u09B0\u09B8\u09CD\u09A5 \u0989\u0987\u09A8\u09CD\u09A1\u09CB\u09B0 \u09B8\u09BE\u09A5\u09C7 \u09B8\u09CD\u09AC\u09AF\u09BC\u0982\u0995\u09CD\u09B0\u09BF\u09AF\u09BC\u09AD\u09BE\u09AC\u09C7 \u09AE\u09BE\u09A8\u09BE\u09A8\u09B8\u0987 \u0995\u09B0\u09C1\u09A8";
		default: return NULL;
		}
	case UiLanguage::Portuguese:
		switch (index)
		{
		case alignCommandIndex: return L"Alinhar tabela (sem alterar largura)";
		case autoAlignTableCommandIndex: return L"Alinhar automaticamente apos edicao (sem alterar largura)";
		case wrapLongCellsCommandIndex: return L"Ajustar largura da tabela a janela";
		case autoFitTableCommandIndex: return L"Ajustar automaticamente largura da tabela a janela";
		default: return NULL;
		}
	case UiLanguage::Russian:
		switch (index)
		{
		case alignCommandIndex: return L"\u0412\u044B\u0440\u043E\u0432\u043D\u044F\u0442\u044C \u0442\u0430\u0431\u043B\u0438\u0446\u0443 (\u0431\u0435\u0437 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u044F \u0448\u0438\u0440\u0438\u043D\u044B)";
		case autoAlignTableCommandIndex: return L"\u0410\u0432\u0442\u043E\u0432\u044B\u0440\u0430\u0432\u043D\u0438\u0432\u0430\u043D\u0438\u0435 \u043F\u043E\u0441\u043B\u0435 \u043F\u0440\u0430\u0432\u043A\u0438 (\u0431\u0435\u0437 \u0438\u0437\u043C\u0435\u043D\u0435\u043D\u0438\u044F \u0448\u0438\u0440\u0438\u043D\u044B)";
		case wrapLongCellsCommandIndex: return L"\u041F\u043E\u0434\u043E\u0433\u043D\u0430\u0442\u044C \u0448\u0438\u0440\u0438\u043D\u0443 \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u043E\u043A\u043D\u043E";
		case autoFitTableCommandIndex: return L"\u0410\u0432\u0442\u043E\u043F\u043E\u0434\u0433\u043E\u043D\u043A\u0430 \u0448\u0438\u0440\u0438\u043D\u044B \u0442\u0430\u0431\u043B\u0438\u0446\u044B \u043F\u043E\u0434 \u043E\u043A\u043D\u043E";
		case narrowColumnCommandIndex: return L"\u0421\u0443\u0437\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446";
		case widenColumnCommandIndex: return L"\u0420\u0430\u0441\u0448\u0438\u0440\u0438\u0442\u044C \u0441\u0442\u043E\u043B\u0431\u0435\u0446";
		default: return NULL;
		}
	case UiLanguage::Indonesian:
		switch (index)
		{
		case alignCommandIndex: return L"Ratakan tabel (tanpa mengubah lebar)";
		case autoAlignTableCommandIndex: return L"Ratakan otomatis setelah edit (tanpa mengubah lebar)";
		case wrapLongCellsCommandIndex: return L"Sesuaikan lebar tabel ke jendela";
		case autoFitTableCommandIndex: return L"Sesuaikan otomatis lebar tabel ke jendela";
		default: return NULL;
		}
	case UiLanguage::Urdu:
		switch (index)
		{
		case alignCommandIndex: return L"\u062C\u062F\u0648\u0644 \u0633\u06CC\u062F\u06BE\u0627 \u06A9\u0631\u06CC\u06BA (\u0686\u0648\u0691\u0627\u0626\u06CC \u0628\u062F\u0644\u06D2 \u0628\u063A\u06CC\u0631)";
		case autoAlignTableCommandIndex: return L"\u062A\u0631\u0645\u06CC\u0645 \u06A9\u06D2 \u0628\u0639\u062F \u062E\u0648\u062F\u06A9\u0627\u0631 \u0633\u06CC\u062F\u06BE (\u0686\u0648\u0691\u0627\u0626\u06CC \u0628\u062F\u0644\u06D2 \u0628\u063A\u06CC\u0631)";
		case wrapLongCellsCommandIndex: return L"\u062C\u062F\u0648\u0644 \u06A9\u06CC \u0686\u0648\u0691\u0627\u0626\u06CC \u0648\u0646\u0688\u0648 \u06A9\u06D2 \u0645\u0637\u0627\u0628\u0642 \u06A9\u0631\u06CC\u06BA";
		case autoFitTableCommandIndex: return L"\u062C\u062F\u0648\u0644 \u06A9\u06CC \u0686\u0648\u0691\u0627\u0626\u06CC \u062E\u0648\u062F\u06A9\u0627\u0631 \u0637\u0648\u0631 \u067E\u0631 \u0648\u0646\u0688\u0648 \u06A9\u06D2 \u0645\u0637\u0627\u0628\u0642 \u06A9\u0631\u06CC\u06BA";
		default: return NULL;
		}
	case UiLanguage::German:
		switch (index)
		{
		case alignCommandIndex: return L"Tabelle ausrichten (Breite unveraendert)";
		case autoAlignTableCommandIndex: return L"Nach Bearbeitung automatisch ausrichten (Breite unveraendert)";
		case wrapLongCellsCommandIndex: return L"Tabellenbreite ans Fenster anpassen";
		case autoFitTableCommandIndex: return L"Tabellenbreite automatisch ans Fenster anpassen";
		default: return NULL;
		}
	case UiLanguage::Japanese:
		switch (index)
		{
		case alignCommandIndex: return L"\u30C6\u30FC\u30D6\u30EB\u3092\u6574\u5217\uFF08\u5E45\u3092\u5909\u66F4\u3057\u306A\u3044\uFF09";
		case autoAlignTableCommandIndex: return L"\u7DE8\u96C6\u5F8C\u306B\u81EA\u52D5\u6574\u5217\uFF08\u5E45\u3092\u5909\u66F4\u3057\u306A\u3044\uFF09";
		case wrapLongCellsCommandIndex: return L"\u30C6\u30FC\u30D6\u30EB\u5E45\u3092\u30A6\u30A3\u30F3\u30C9\u30A6\u306B\u5408\u308F\u305B\u308B";
		case autoFitTableCommandIndex: return L"\u30C6\u30FC\u30D6\u30EB\u5E45\u3092\u30A6\u30A3\u30F3\u30C9\u30A6\u306B\u81EA\u52D5\u8ABF\u6574";
		default: return NULL;
		}
	case UiLanguage::NigerianPidgin:
		switch (index)
		{
		case alignCommandIndex: return L"Arrange table (no change width)";
		case autoAlignTableCommandIndex: return L"Auto arrange after edit (no change width)";
		case wrapLongCellsCommandIndex: return L"Fit table width to window";
		case autoFitTableCommandIndex: return L"Auto fit table width to window";
		default: return NULL;
		}
	case UiLanguage::Marathi:
		switch (index)
		{
		case alignCommandIndex: return L"\u0924\u0915\u094D\u0924\u093E \u0938\u0902\u0930\u0947\u0916\u093F\u0924 \u0915\u0930\u093E (\u0930\u0941\u0902\u0926\u0940 \u0928 \u092C\u0926\u0932\u0924\u093E)";
		case autoAlignTableCommandIndex: return L"\u0938\u0902\u092A\u093E\u0926\u0928\u093E\u0928\u0902\u0924\u0930 \u0938\u094D\u0935\u092F\u0902\u091A\u0932\u093F\u0924 \u0938\u0902\u0930\u0947\u0916\u0928 (\u0930\u0941\u0902\u0926\u0940 \u0928 \u092C\u0926\u0932\u0924\u093E)";
		case wrapLongCellsCommandIndex: return L"\u0924\u0915\u094D\u0924\u094D\u092F\u093E\u091A\u0940 \u0930\u0941\u0902\u0926\u0940 \u0935\u093F\u0902\u0921\u094B\u0932\u093E \u092C\u0938\u0935\u093E";
		case autoFitTableCommandIndex: return L"\u0924\u0915\u094D\u0924\u094D\u092F\u093E\u091A\u0940 \u0930\u0941\u0902\u0926\u0940 \u0935\u093F\u0902\u0921\u094B\u0932\u093E \u0906\u092A\u094B\u0906\u092A \u092C\u0938\u0935\u093E";
		default: return NULL;
		}
	case UiLanguage::Telugu:
		switch (index)
		{
		case alignCommandIndex: return L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15\u0C28\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41 (\u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41 \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C15\u0C41\u0C02\u0C21\u0C3E)";
		case autoAlignTableCommandIndex: return L"\u0C38\u0C35\u0C30\u0C23 \u0C24\u0C30\u0C4D\u0C35\u0C3E\u0C24 \u0C38\u0C4D\u0C35\u0C2F\u0C02\u0C1A\u0C3E\u0C32\u0C15 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41 (\u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41 \u0C2E\u0C3E\u0C30\u0C4D\u0C1A\u0C15\u0C41\u0C02\u0C21\u0C3E)";
		case wrapLongCellsCommandIndex: return L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15 \u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41\u0C28\u0C41 \u0C35\u0C3F\u0C02\u0C21\u0C4B\u0C15\u0C41 \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41";
		case autoFitTableCommandIndex: return L"\u0C2A\u0C1F\u0C4D\u0C1F\u0C3F\u0C15 \u0C35\u0C46\u0C21\u0C32\u0C4D\u0C2A\u0C41\u0C28\u0C41 \u0C35\u0C3F\u0C02\u0C21\u0C4B\u0C15\u0C41 \u0C38\u0C4D\u0C35\u0C2F\u0C02\u0C1A\u0C3E\u0C32\u0C15\u0C02\u0C17\u0C3E \u0C38\u0C30\u0C3F\u0C2A\u0C30\u0C1A\u0C41";
		default: return NULL;
		}
	case UiLanguage::Turkish:
		switch (index)
		{
		case alignCommandIndex: return L"Tabloyu hizala (genisligi degistirme)";
		case autoAlignTableCommandIndex: return L"Duzenlemeden sonra otomatik hizala (genisligi degistirme)";
		case wrapLongCellsCommandIndex: return L"Tablo genisligini pencereye sigdir";
		case autoFitTableCommandIndex: return L"Tablo genisligini pencereye otomatik sigdir";
		default: return NULL;
		}
	case UiLanguage::Tamil:
		switch (index)
		{
		case alignCommandIndex: return L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8\u0BAF\u0BC8 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1 (\u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BBE\u0BAE\u0BB2\u0BCD)";
		case autoAlignTableCommandIndex: return L"\u0BA4\u0BBF\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BA4\u0BCD\u0BA4\u0BBF\u0BB1\u0BCD\u0B95\u0BC1\u0BAA\u0BCD \u0BAA\u0BBF\u0BB1\u0B95\u0BC1 \u0BA4\u0BBE\u0BA9\u0BBE\u0B95 \u0B92\u0BB4\u0BC1\u0B99\u0BCD\u0B95\u0BC1\u0BAA\u0B9F\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1 (\u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0BAE\u0BBE\u0BB1\u0BCD\u0BB1\u0BBE\u0BAE\u0BB2\u0BCD)";
		case wrapLongCellsCommandIndex: return L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0B9A\u0BBE\u0BB3\u0BB0\u0BA4\u0BCD\u0BA4\u0BBF\u0BB1\u0BCD\u0B95\u0BC1 \u0BAA\u0BCA\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1";
		case autoFitTableCommandIndex: return L"\u0B85\u0B9F\u0BCD\u0B9F\u0BB5\u0BA3\u0BC8 \u0B85\u0B95\u0BB2\u0BA4\u0BCD\u0BA4\u0BC8 \u0B9A\u0BBE\u0BB3\u0BB0\u0BA4\u0BCD\u0BA4\u0BBF\u0BB1\u0BCD\u0B95\u0BC1 \u0BA4\u0BBE\u0BA9\u0BBE\u0B95 \u0BAA\u0BCA\u0BB0\u0BC1\u0BA4\u0BCD\u0BA4\u0BC1";
		default: return NULL;
		}
	case UiLanguage::YueChinese:
		switch (index)
		{
		case alignCommandIndex: return L"\u5C0D\u9F4A\u8868\u683C\uFF08\u4E0D\u6539\u8B8A\u95CA\u5EA6\uFF09";
		case autoAlignTableCommandIndex: return L"\u7DE8\u8F2F\u5F8C\u81EA\u52D5\u5C0D\u9F4A\uFF08\u4E0D\u6539\u8B8A\u95CA\u5EA6\uFF09";
		case wrapLongCellsCommandIndex: return L"\u5C07\u8868\u683C\u95CA\u5EA6\u914D\u5408\u8996\u7A97";
		case autoFitTableCommandIndex: return L"\u81EA\u52D5\u5C07\u8868\u683C\u95CA\u5EA6\u914D\u5408\u8996\u7A97";
		default: return NULL;
		}
	case UiLanguage::Vietnamese:
		switch (index)
		{
		case alignCommandIndex: return L"C\u0103n ch\u1EC9nh b\u1EA3ng (kh\u00F4ng \u0111\u1ED5i chi\u1EC1u r\u1ED9ng)";
		case autoAlignTableCommandIndex: return L"T\u1EF1 \u0111\u1ED9ng c\u0103n ch\u1EC9nh sau khi s\u1EEDa (kh\u00F4ng \u0111\u1ED5i chi\u1EC1u r\u1ED9ng)";
		case wrapLongCellsCommandIndex: return L"V\u1EEBa chi\u1EC1u r\u1ED9ng b\u1EA3ng v\u1EDBi c\u1EEDa s\u1ED5";
		case autoFitTableCommandIndex: return L"T\u1EF1 \u0111\u1ED9ng v\u1EEBa chi\u1EC1u r\u1ED9ng b\u1EA3ng v\u1EDBi c\u1EEDa s\u1ED5";
		default: return NULL;
		}
	default:
		switch (index)
		{
		case alignCommandIndex: return L"Align table (no width change)";
		case autoAlignTableCommandIndex: return L"Auto align after edit (no width change)";
		case wrapLongCellsCommandIndex: return L"Fit table width to window";
		case autoFitTableCommandIndex: return L"Auto fit table width to window";
		case narrowColumnCommandIndex: return L"Narrow column";
		case widenColumnCommandIndex: return L"Widen column";
		default: return NULL;
		}
	}
}

const wchar_t *PluginLocalization::autoModeCommandText(std::size_t index, const wchar_t *text)
{
	if (!text)
		return text;
	const wchar_t *prefix = NULL;
	switch (index)
	{
	case autoAlignTableCommandIndex:
		prefix = L"Light ";
		break;
	case autoFitTableCommandIndex:
		prefix = L"Power ";
		break;
	default:
		return text;
	}

	modeText_[index] = prefix;
	modeText_[index] += text;
	return modeText_[index].c_str();
}

const wchar_t *PluginLocalization::commandText(std::size_t index)
{
	if (const wchar_t *special = localizedSpecialCommandText(index))
		return autoModeCommandText(index, special);

	const UiText &localized = uiText();
	const std::size_t legacy = legacyCommandIndex(index);
	if (legacy >= static_cast<std::size_t>(commandCount))
		return autoModeCommandText(index, englishUiText.commands[0]);
	if (legacy < static_cast<std::size_t>(commandCount) && localized.commands[legacy])
		return autoModeCommandText(index, localized.commands[legacy]);
	return autoModeCommandText(index, englishUiText.commands[legacy]);
}

const wchar_t *PluginLocalization::commandMenuText(std::size_t index)
{
	const wchar_t *text = commandText(index);
	const wchar_t *shortcut = commandShortcutText(index);
	if (!shortcut || !*shortcut || index >= static_cast<std::size_t>(commandCount))
		return text;

	menuText_[index] = text;
	menuText_[index] += L"\t";
	menuText_[index] += shortcut;
	return menuText_[index].c_str();
}
}
