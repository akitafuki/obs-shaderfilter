#include "obs-shaderfilter.h"

static bool is_var_char(char ch)
{
	return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

static void convert_if_defined(struct dstr *effect_text)
{
	char *pos = strstr(effect_text->array, "#if defined(");
	while (pos) {
		size_t diff = pos - effect_text->array;
		char *ch = strstr(effect_text->array + diff + 12, ")\n");
		pos = strstr(effect_text->array + diff + 12, "#if defined(");
		if (ch && (!pos || ch < pos)) {
			*ch = ' ';
			dstr_remove(effect_text, diff, 12);
			dstr_insert(effect_text, diff, "#ifdef ");
			pos = strstr(effect_text->array + diff + 5, "#if defined(");
		}
	}
}

static void convert_atan(struct dstr *effect_text)
{
	char *pos = strstr(effect_text->array, "atan(");
	while (pos) {
		if (is_var_char(*(pos - 1))) {
			pos = strstr(pos + 5, "atan(");
			continue;
		}
		size_t diff = pos - effect_text->array;
		char *comma = strstr(pos + 5, ",");
		char *divide = strstr(pos + 5, "/");
		if (!comma && !divide)
			return;

		int depth = 0;
		char *open = strstr(pos + 5, "(");
		char *close = strstr(pos + 5, ")");
		if (!close)
			return;
		do {
			while (open && open < close) {
				depth++;
				open = strstr(open + 1, "(");
			}
			while (depth > 0 && close && (!open || close < open)) {
				depth--;
				if (depth == 0 && (!comma || comma < close))
					comma = strstr(close + 1, ",");
				if (depth == 0 && (!divide || divide < close))
					divide = strstr(close + 1, "/");
				if (comma && open && comma > open && comma < close)
					comma = NULL;
				if (divide && open && divide > open && divide < close)
					divide = NULL;

				open = strstr(close + 1, "(");
				close = strstr(close + 1, ")");
			}
		} while (depth > 0 && close);

		if (close && comma && comma < close && (!open || comma < open)) {
			//*comma = '/';
			dstr_insert(effect_text, diff + 4, "2");
		}
		if (close && divide && divide < close && (!open || divide < open)) {
			*divide = ',';
			dstr_insert(effect_text, diff + 4, "2");
		}

		pos = strstr(effect_text->array + diff + 5, "atan(");
	}
}

static void insert_begin_of_block(struct dstr *effect_text, size_t diff, char *insert)
{
	int depth = 0;
	char *ch = effect_text->array + diff;
	while (ch > effect_text->array && (*ch == ' ' || *ch == '\t'))
		ch--;
	while (ch > effect_text->array) {
		while (*ch != '=' && *ch != '(' && *ch != ')' && *ch != '+' && *ch != '-' && *ch != '/' && *ch != '*' &&
		       *ch != ' ' && *ch != '\t' && ch > effect_text->array)
			ch--;
		if (*ch == '(') {
			if (depth == 0) {
				dstr_insert(effect_text, ch - effect_text->array + 1, insert);
				return;
			}
			ch--;
			depth--;
		} else if (*ch == ')') {
			ch--;
			depth++;
		} else if (*ch == '=') {
			dstr_insert(effect_text, ch - effect_text->array + 1, insert);
			return;
		} else if (*ch == '+' || *ch == '-' || *ch == '/' || *ch == '*' || *ch == ' ' || *ch == '\t') {
			if (depth == 0) {
				dstr_insert(effect_text, ch - effect_text->array + 1, insert);
				return;
			}
			ch--;
		}
	}
}

static void insert_end_of_block(struct dstr *effect_text, size_t diff, char *insert)
{
	int depth = 0;
	char *ch = effect_text->array + diff;
	while (*ch == ' ' || *ch == '\t')
		ch++;
	while (*ch != 0) {
		while (*ch != ';' && *ch != '(' && *ch != ')' && *ch != '+' && *ch != '-' && *ch != '/' && *ch != '*' &&
		       *ch != ' ' && *ch != '\t' && *ch != ',' && *ch != 0)
			ch++;
		if (*ch == '(') {
			ch++;
			depth++;
		} else if (*ch == ')') {
			if (depth == 0) {
				dstr_insert(effect_text, ch - effect_text->array, insert);
				return;
			}
			ch++;
			depth--;
		} else if (*ch == ';') {
			dstr_insert(effect_text, ch - effect_text->array, insert);
			return;
		} else if (*ch == '+' || *ch == '-' || *ch == '/' || *ch == '*' || *ch == ' ' || *ch == '\t' || *ch == ',') {
			if (depth == 0) {
				dstr_insert(effect_text, ch - effect_text->array, insert);
				return;
			}
			ch++;
		}
	}
}

static void convert_mat_mul_var(struct dstr *effect_text, struct dstr *var_function_name)
{
	char *pos = strstr(effect_text->array, var_function_name->array);
	while (pos) {
		if (is_var_char(*(pos - 1)) || is_var_char(*(pos + var_function_name->len))) {
			pos = strstr(pos + var_function_name->len, var_function_name->array);
			continue;
		}
		size_t diff = pos - effect_text->array;
		char *ch = pos + var_function_name->len;
		while (*ch == ' ' || *ch == '\t')
			ch++;
		if (*ch == '*' && *(ch + 1) != '=') {
			size_t diff3 = ch - effect_text->array + 1;
			*ch = ',';
			insert_end_of_block(effect_text, diff3, ")");
			dstr_insert(effect_text, diff, "mul(");
			pos = strstr(effect_text->array + diff + 4 + var_function_name->len, var_function_name->array);
			continue;
		}
		ch = pos - 1;
		while ((*ch == ' ' || *ch == '\t') && ch > effect_text->array)
			ch--;
		if (*ch == '=' && *(ch - 1) == '*') {
			char *end = ch - 2;
			while ((*end == ' ' || *end == '\t') && end > effect_text->array)
				end--;
			char *start = end;
			while ((is_var_char(*start) || *start == '.') && start > effect_text->array)
				start--;
			start++;
			end++;

			diff = ch - 1 - effect_text->array;

			struct dstr insert = {0};
			dstr_init_copy(&insert, "= mul(");
			dstr_ncat(&insert, start, end - start);
			dstr_cat(&insert, ",");
			dstr_remove(effect_text, diff, 2);
			dstr_insert(effect_text, diff, insert.array);

			char *line_end = effect_text->array + diff;
			while (*line_end != ';' && *line_end != 0)
				line_end++;

			dstr_insert(effect_text, line_end - effect_text->array, ")");

			pos = strstr(effect_text->array + diff + insert.len + 1 + var_function_name->len, var_function_name->array);
			dstr_free(&insert);

			continue;
		} else if (*ch == '*') {
			size_t diff2 = ch - effect_text->array - 1;
			*ch = ',';
			insert_end_of_block(effect_text, diff + var_function_name->len, ")");
			insert_begin_of_block(effect_text, diff2, "mul(");

			pos = strstr(effect_text->array + diff + 4 + var_function_name->len, var_function_name->array);
			continue;
		}

		pos = strstr(effect_text->array + diff + var_function_name->len, var_function_name->array);
	}
}

static void convert_mat_mul(struct dstr *effect_text, char *var_type)
{
	size_t len = strlen(var_type);
	char *pos = strstr(effect_text->array, var_type);
	while (pos) {
		if (is_var_char(*(pos - 1))) {
			pos = strstr(pos + len, var_type);
			continue;
		}
		size_t diff = pos - effect_text->array;
		char *begin = pos + len;
		if (*begin == '(') {
			char *ch = pos - 1;
			while ((*ch == ' ' || *ch == '\t') && ch > effect_text->array)
				ch--;
			if (*ch == '=' && *(ch - 1) == '*') {
				// mat constructor with *= in front of it
				char *end = ch - 2;
				while ((*end == ' ' || *end == '\t') && end > effect_text->array)
					end--;
				char *start = end;
				while ((is_var_char(*start) || *start == '.') && start > effect_text->array)
					start--;
				start++;
				end++;

				size_t diff2 = ch - effect_text->array - 1;

				struct dstr insert = {0};
				dstr_init_copy(&insert, "= mul(");
				dstr_ncat(&insert, start, end - start);
				dstr_cat(&insert, ",");
				dstr_remove(effect_text, diff2, 2);
				dstr_insert(effect_text, diff2, insert.array);

				char *line_end = effect_text->array + diff2;
				while (*line_end != ';' && *line_end != 0)
					line_end++;

				dstr_insert(effect_text, line_end - effect_text->array, ")");

				pos = strstr(effect_text->array + diff + insert.len + len + 1, var_type);
				dstr_free(&insert);

				continue;
			} else if (*ch == '*') {
				// mat constructor with * in front of it
				size_t diff2 = ch - effect_text->array - 1;
				*ch = ',';
				insert_end_of_block(effect_text, diff + len, ")");
				insert_begin_of_block(effect_text, diff2, "mul(");

				pos = strstr(effect_text->array + diff + len + 4, var_type);
				continue;
			}

			int depth = 1;
			ch = begin + 1;
			while (*ch != 0) {
				while (*ch != ';' && *ch != '(' && *ch != ')' && *ch != '+' && *ch != '-' && *ch != '/' &&
				       *ch != '*' && *ch != 0)
					ch++;
				if (*ch == '(') {
					ch++;
					depth++;
				} else if (*ch == ')') {
					if (depth == 0) {
						break;
					}
					ch++;
					depth--;
				} else if (*ch == '*') {
					if (depth == 0) {
						//mat constructor follow by *
						*ch = ',';
						insert_end_of_block(effect_text, ch - effect_text->array + 1, ")");
						dstr_insert(effect_text, diff, "mul(");
						break;
					}
					ch++;
				} else if (*ch == ';') {
					break;
				} else if (depth == 0) {
					break;
				} else if (*ch != 0) {
					ch++;
				}
			}
		}
		if (*begin != ' ' && *begin != '\t') {
			pos = strstr(pos + len, var_type);
			continue;
		}
		while (*begin == ' ' || *begin == '\t')
			begin++;
		if (!is_var_char(*begin)) {
			pos = strstr(pos + len, var_type);
			continue;
		}
		char *end = begin;
		while (is_var_char(*end))
			end++;
		struct dstr var_function_name = {0};
		dstr_ncat(&var_function_name, begin, end - begin);

		convert_mat_mul_var(effect_text, &var_function_name);
		dstr_free(&var_function_name);

		pos = strstr(effect_text->array + diff + len, var_type);
	}
}

static bool is_in_function(struct dstr *effect_text, size_t diff)
{
	char *pos = effect_text->array + diff;
	int depth = 0;
	while (depth >= 0) {
		char *end = strstr(pos, "}");
		char *begin = strstr(pos, "{");
		if (end && begin && begin < end) {
			pos = begin + 1;
			depth++;
		} else if (end && begin && end < begin) {
			pos = end + 1;
			depth--;
		} else if (end) {
			pos = end + 1;
			depth--;
		} else if (begin && depth < 0) {
			break;
		} else if (begin) {
			pos = begin + 1;
			depth++;
		} else {
			break;
		}
	}
	return depth != 0;
}

static void convert_init(struct dstr *effect_text, char *name)
{
	const size_t len = strlen(name);
	char extra = 0;
	char *pos = strstr(effect_text->array, name);
	while (pos) {
		size_t diff = pos - effect_text->array;
		if (pos > effect_text->array && is_var_char(*(pos - 1))) {
			pos = strstr(effect_text->array + diff + len, name);
			continue;
		}
		char *ch = pos + len;
		if (*ch >= '2' && *ch <= '9') {
			extra = *ch;
			ch++;
		} else {
			extra = 0;
		}
		if (*ch != ' ' && *ch != '\t') {
			pos = strstr(effect_text->array + diff + len, name);
			continue;
		}
		char *begin = pos - 1;
		while (begin > effect_text->array && (*begin == ' ' || *begin == '\t'))
			begin--;
		if (*begin == '(' || *begin == ',' ||
		    (is_var_char(*begin) && memcmp("uniform", begin - 6, 7) != 0 && memcmp("const", begin - 4, 5) != 0)) {
			pos = strstr(effect_text->array + diff + len, name);
			continue;
		}
		if (!is_in_function(effect_text, diff)) {
			while (true) {
				while (*ch != 0 && (*ch == ' ' || *ch == '\t'))
					ch++;
				while (is_var_char(*ch))
					ch++;
				while (*ch != 0 && (*ch == ' ' || *ch == '\t'))
					ch++;
				if (*ch != ',')
					break;
				*ch = ';';
				diff = ch - effect_text->array;
				dstr_insert(effect_text, diff + 1, " ");
				if (extra) {
					dstr_insert_ch(effect_text, diff + 1, extra);
				}
				dstr_insert(effect_text, diff + 1, name);
				if (memcmp("const", begin - 4, 5) == 0) {
					dstr_insert(effect_text, diff + 1, "\nconst ");
					diff += 9;
				} else {
					dstr_insert(effect_text, diff + 1, "\nuniform ");
					diff += 11;
				}
				if (extra)
					diff++;
				ch = effect_text->array + diff + len;
			}

			if ((*ch == '=' && *(ch + 1) != '=') || *ch == ';') {
				if (memcmp("uniform", begin - 6, 7) != 0 && memcmp("const", begin - 4, 5) != 0) {
					dstr_insert(effect_text, begin - effect_text->array + 1, "uniform ");
					diff += 8;
				}
			}
		}
		pos = strstr(effect_text->array + diff + len, name);
	}
}

static void convert_vector_init(struct dstr *effect_text, char *name, int count)
{
	const size_t len = strlen(name);

	char *pos = strstr(effect_text->array, name);
	while (pos) {
		size_t diff = pos - effect_text->array;
		char *ch = pos + len;
		int depth = 0;
		int function_depth = -1;
		bool only_one = true;
		bool only_numbers = true;
		bool only_float = true;
		while (*ch != 0 && (only_numbers || only_float)) {
			if (*ch == '(') {
				depth++;
			} else if (*ch == ')') {
				if (depth == 0) {
					break;
				}
				depth--;
				if (depth == function_depth) {
					function_depth = -1;
				}
			} else if (*ch == ',') {
				if (depth == 0) {
					only_one = false;
				}
			} else if (*ch == ';') {
				only_one = false;
				break;
			} else if (is_var_char(*ch) && (*ch < '0' || *ch > '9')) {
				only_numbers = false;
				char *begin = ch;
				while (is_var_char(*ch))
					ch++;
				if (*ch == '.') {
					size_t c = 1;
					while (is_var_char(*(ch + c)))
						c++;
					if (c != 2 && function_depth < 0)
						only_float = false;
					ch += c - 1;
				} else if (function_depth >= 0) {
					ch--;
				} else if (*ch == '(' &&
					   (strncmp(begin, "length(", 7) == 0 || strncmp(begin, "float(", 6) == 0 ||
					    strncmp(begin, "uint(", 5) == 0 || strncmp(begin, "int(", 4) == 0 ||
					    strncmp(begin, "asfloat(", 8) == 0 || strncmp(begin, "asdouble(", 9) == 0 ||
					    strncmp(begin, "asint(", 6) == 0 || strncmp(begin, "asuint(", 7) == 0 ||
					    strncmp(begin, "determinant(", 12) == 0 || strncmp(begin, "distance(", 9) == 0 ||
					    strncmp(begin, "dot(", 4) == 0 || strncmp(begin, "countbits(", 10) == 0 ||
					    strncmp(begin, "firstbithigh(", 13) == 0 || strncmp(begin, "firstbitlow(", 12) == 0 ||
					    strncmp(begin, "reversebits(", 12) == 0)) {
					function_depth = depth;
					depth++;
				} else if (*ch == '(' &&
					   (strncmp(begin, "abs(", 4) == 0 || strncmp(begin, "acos(", 5) == 0 ||
					    strncmp(begin, "asin(", 5) == 0 || strncmp(begin, "atan(", 5) == 0 ||
					    strncmp(begin, "atan2(", 6) == 0 || strncmp(begin, "ceil(", 5) == 0 ||
					    strncmp(begin, "clamp(", 6) == 0 || strncmp(begin, "cos(", 4) == 0 ||
					    strncmp(begin, "cosh(", 5) == 0 || strncmp(begin, "ddx(", 4) == 0 ||
					    strncmp(begin, "ddy(", 4) == 0 || strncmp(begin, "degrees(", 8) == 0 ||
					    strncmp(begin, "exp(", 4) == 0 || strncmp(begin, "exp2(", 5) == 0 ||
					    strncmp(begin, "floor(", 6) == 0 || strncmp(begin, "fma(", 4) == 0 ||
					    strncmp(begin, "fmod(", 5) == 0 || strncmp(begin, "frac(", 5) == 0 ||
					    strncmp(begin, "frexp(", 6) == 0 || strncmp(begin, "fwidth(", 7) == 0 ||
					    strncmp(begin, "ldexp(", 6) == 0 || strncmp(begin, "lerp(", 5) == 0 ||
					    strncmp(begin, "log(", 4) == 0 || strncmp(begin, "log10(", 6) == 0 ||
					    strncmp(begin, "log2(", 5) == 0 || strncmp(begin, "mad(", 4) == 0 ||
					    strncmp(begin, "max(", 4) == 0 || strncmp(begin, "min(", 4) == 0 ||
					    strncmp(begin, "modf(", 5) == 0 || strncmp(begin, "mod(", 4) == 0 ||
					    strncmp(begin, "mul(", 4) == 0 || strncmp(begin, "normalize(", 10) == 0 ||
					    strncmp(begin, "pow(", 4) == 0 || strncmp(begin, "radians(", 8) == 0 ||
					    strncmp(begin, "rcp(", 4) == 0 || strncmp(begin, "reflect(", 8) == 0 ||
					    strncmp(begin, "refract(", 8) == 0 || strncmp(begin, "round(", 6) == 0 ||
					    strncmp(begin, "rsqrt(", 6) == 0 || strncmp(begin, "saturate(", 9) == 0 ||
					    strncmp(begin, "sign(", 5) == 0 || strncmp(begin, "sin(", 4) == 0 ||
					    strncmp(begin, "sincos(", 7) == 0 || strncmp(begin, "sinh(", 5) == 0 ||
					    strncmp(begin, "smoothstep(", 11) == 0 || strncmp(begin, "sqrt(", 5) == 0 ||
					    strncmp(begin, "step(", 5) == 0 || strncmp(begin, "tan(", 4) == 0 ||
					    strncmp(begin, "tanh(", 5) == 0 || strncmp(begin, "transpose(", 10) == 0 ||
					    strncmp(begin, "trunc(", 6) == 0)) {
					depth++;
				} else {
					struct dstr find = {0};
					bool found = false;
					dstr_copy(&find, "float ");
					dstr_ncat(&find, begin, ch - begin + (*ch == '(' ? 1 : 0));
					char *t = strstr(effect_text->array, find.array);
					while (t != NULL) {
						t += find.len;
						if (*ch == '(') {
							found = true;
							break;
						} else if (!is_var_char(*t)) {
							found = true;
							break;
						}
						t = strstr(t, find.array);
					}
					if (!found) {
						dstr_copy(&find, "int ");
						dstr_ncat(&find, begin, ch - begin + (*ch == '(' ? 1 : 0));
						t = strstr(effect_text->array, find.array);
						while (t != NULL) {
							t += find.len;
							if (*ch == '(') {
								found = true;
								break;
							} else if (!is_var_char(*t)) {
								found = true;
								break;
							}
							t = strstr(t, find.array);
						}
					}
					if (!found && *ch != '(') {
						dstr_copy(&find, "#define ");
						dstr_ncat(&find, begin, ch - begin);
						char *t = strstr(effect_text->array, find.array);
						while (t != NULL) {
							t += find.len;
							if (!is_var_char(*t)) {
								while (*t == ' ' || *t == '\t')
									t++;
								if (*t >= '0' && *t <= '9') {
									found = true;
									break;
								}
							}
							t = strstr(t, find.array);
						}
					}
					if (!found) {
						only_float = false;
					} else if (*ch == '(') {
						function_depth = depth;
					}
					dstr_free(&find);
					ch--;
				}
			} else if ((*ch < '0' || *ch > '9') && *ch != '.' && *ch != ' ' && *ch != '\t' && *ch != '+' &&
				   *ch != '-' && *ch != '*' && *ch != '/') {
				only_numbers = false;
			}
			ch++;
		}
		size_t end_diff = ch - effect_text->array;
		if (count > 1 && only_one && (only_numbers || only_float)) {
			//only 1 simple arg in the float4
			struct dstr found = {0};
			dstr_init(&found);
			dstr_ncat(&found, pos, ch - pos + 1);

			struct dstr replacement = {0};
			dstr_init_copy(&replacement, name);
			dstr_ncat(&replacement, pos + len, ch - (pos + len));
			for (int i = 1; i < count; i++) {
				dstr_cat(&replacement, ",");
				dstr_ncat(&replacement, pos + len, ch - (pos + len));
			}
			dstr_cat(&replacement, ")");

			dstr_replace(effect_text, found.array, replacement.array);

			end_diff -= found.len;
			end_diff += replacement.len;
			dstr_free(&replacement);
			dstr_free(&found);
		}

		if (!is_in_function(effect_text, diff)) {
			char *begin = effect_text->array + diff - 1;
			while (begin > effect_text->array && (*begin == ' ' || *begin == '\t'))
				begin--;
			if (*begin == '=') {
				begin--;
				while (begin > effect_text->array && (*begin == ' ' || *begin == '\t'))
					begin--;
				while (is_var_char(*begin))
					begin--;
				while (begin > effect_text->array && (*begin == ' ' || *begin == '\t'))
					begin--;
				if (memcmp(name, begin - len + 2, len - 1) == 0) {

					begin -= len - 1;
					while (begin > effect_text->array && (*begin == ' ' || *begin == '\t'))
						begin--;
					if (memcmp("uniform", begin - 6, 7) != 0 && memcmp("const", begin - 4, 5) != 0) {
						dstr_insert(effect_text, begin - effect_text->array + 1, "uniform ");
						diff += 8;
						end_diff += 8;
					}
					if (effect_text->array[end_diff] == ')') {
						if (count > 1) {
							effect_text->array[end_diff] = '}';
							dstr_remove(effect_text, diff, len);
							dstr_insert(effect_text, diff, "{");
						} else {
							dstr_remove(effect_text, end_diff, 1);
							dstr_remove(effect_text, diff, len);
						}
					}
				}
			}
		}

		pos = strstr(effect_text->array + diff + len, name);
	}
}

static void convert_if0(struct dstr *effect_text)
{
	char *begin = strstr(effect_text->array, "#if 0");
	while (begin) {
		size_t diff = begin - effect_text->array;
		char *end = strstr(begin, "#endif");
		if (!end)
			return;
		char *el = strstr(begin, "#else");
		char *eli = strstr(begin, "#elif");
		if (eli && eli < end && (!el || eli < el)) {
			//replace #elif with #if
			dstr_remove(effect_text, diff, el - begin + 5);
			dstr_insert(effect_text, diff, "#if");
			begin = strstr(effect_text->array + diff + 3, "#if 0");
		} else if (el && el < end) {
			dstr_remove(effect_text, end - effect_text->array,
				    6); // #endif
			dstr_remove(effect_text, diff, el - begin + 5);
			begin = strstr(effect_text->array + diff, "#if 0");
		} else if (!el || el > end) {
			dstr_remove(effect_text, diff, end - begin + 6);
			begin = strstr(effect_text->array + diff, "#if 0");
		} else {
			begin = strstr(effect_text->array + diff + 5, "#if 0");
		}
	}
}

static void convert_if1(struct dstr *effect_text)
{
	char *begin = strstr(effect_text->array, "#if 1");
	while (begin) {
		size_t diff = begin - effect_text->array;
		char *end = strstr(begin, "#endif");
		if (!end)
			return;
		char *el = strstr(begin, "#el");
		if (el && el < end) {
			dstr_remove(effect_text, el - effect_text->array,
				    end - el + 6); // #endif
			end = strstr(effect_text->array + diff, "\n");
			if (end)
				dstr_remove(effect_text, diff, end - (effect_text->array + diff));
			begin = strstr(effect_text->array + diff, "#if 1");
		} else if (!el || el > end) {
			dstr_remove(effect_text, end - effect_text->array, 6);
			end = strstr(effect_text->array + diff, "\n");
			if (end)
				dstr_remove(effect_text, diff, end - (effect_text->array + diff));
			begin = strstr(effect_text->array + diff, "#if 1");
		} else {
			begin = strstr(effect_text->array + diff + 5, "#if 1");
		}
	}
}

static void convert_define(struct dstr *effect_text)
{
	char *pos = strstr(effect_text->array, "#define ");
	while (pos) {
		size_t diff = pos - effect_text->array;
		char *start = pos + 8;
		while (*start == ' ' || *start == '\t')
			start++;
		char *end = start;
		while (*end != ' ' && *end != '\t' && *end != '\n' && *end != 0)
			end++;
		char *t = strstr(start, "(");
		if (t && t < end) {
			// don't replace macro
			pos = strstr(effect_text->array + diff + 8, "#define ");
			continue;
		}

		struct dstr def_name = {0};
		dstr_ncat(&def_name, start, end - start);

		start = end;
		while (*start == ' ' || *start == '\t')
			start++;

		end = start;
		while (*end != '\n' && *end != 0 && (*end != '/' || *(end + 1) != '/'))
			end++;

		t = strstr(start, "(");
		if (*start == '(' || (t && t < end)) {
			struct dstr replacement = {0};
			dstr_ncat(&replacement, start, end - start);

			dstr_remove(effect_text, diff, end - (effect_text->array + diff));

			dstr_replace(effect_text, def_name.array, replacement.array);

			dstr_free(&replacement);
			pos = strstr(effect_text->array + diff, "#define ");
		} else {
			pos = strstr(effect_text->array + diff + 8, "#define ");
		}
		dstr_free(&def_name);
	}
}

static void convert_return(struct dstr *effect_text, struct dstr *var_name, size_t main_diff)
{
	size_t count = 0;
	char *pos = strstr(effect_text->array + main_diff, var_name->array);
	while (pos) {
		if (is_var_char(*(pos - 1)) || (*(pos - 1) == '/' && *(pos - 2) == '/')) {
			pos = strstr(pos + var_name->len, var_name->array);
			continue;
		}
		size_t diff = pos - effect_text->array;
		char *ch = pos + var_name->len;
		if (*ch == '.') {
			ch++;
			while (is_var_char(*ch))
				ch++;
		}
		while (*ch == ' ' || *ch == '\t')
			ch++;

		if (*ch == '=' || (*(ch + 1) == '=' && (*ch == '*' || *ch == '/' || *ch == '+' || *ch == '-'))) {
			char *bpos = pos - 1;
			while (*bpos != '\n' && (*bpos != '/' || *(bpos + 1) != '/') && bpos > effect_text->array)
				bpos--;
			if (*bpos == '/' && *(bpos + 1) == '/') {
				//comment line
			} else {
				count++;
			}
		}

		pos = strstr(effect_text->array + diff + var_name->len, var_name->array);
	}
	if (count == 0)
		return;
	if (count == 1) {
		pos = strstr(effect_text->array + main_diff, var_name->array);
		while (pos) {
			if (is_var_char(*(pos - 1))) {
				pos = strstr(pos + var_name->len, var_name->array);
				continue;
			}
			char *bpos = pos - 1;
			while (*bpos != '\n' && (*bpos != '/' || *(bpos + 1) != '/') && bpos > effect_text->array)
				bpos--;
			if (*bpos == '/' && *(bpos + 1) == '/') {
				pos = strstr(pos + var_name->len, var_name->array);
				continue;
			}

			size_t diff = pos - effect_text->array;
			char *ch = pos + var_name->len;
			bool contains_alpha = true;
			if (*ch == '.') {
				contains_alpha = false;
				ch++;
				while (is_var_char(*ch)) {
					if (*ch == 'a' || *ch == 'A' || *ch == 'w' || *ch == 'W')
						contains_alpha = true;
					ch++;
				}
			}

			while (*ch == ' ' || *ch == '\t')
				ch++;

			if (*ch == '=') {
				dstr_remove(effect_text, diff, ch - pos + 1);
				if (!contains_alpha) {
					dstr_insert(effect_text, diff, "return float4(");
					while (diff < effect_text->len && effect_text->array[diff] != ';')
						diff++;
					dstr_insert(effect_text, diff, ",1.0)");
				} else {
					dstr_insert(effect_text, diff, "return ");
				}
				return;
			} else if (*(ch + 1) == '=' && (*ch == '*' || *ch == '/' || *ch == '+' || *ch == '-')) {
				dstr_remove(effect_text, diff, ch - pos + 2);
				if (!contains_alpha) {
					dstr_insert(effect_text, diff, "return float4(");
					while (diff < effect_text->len && effect_text->array[diff] != ';')
						diff++;
					dstr_insert(effect_text, diff, ",1.0)");
				} else {
					dstr_insert(effect_text, diff, "return ");
				}
				return;
			}

			pos = strstr(effect_text->array + diff + var_name->len, var_name->array);
		}
		return;
	}

	size_t replaced = 0;
	size_t start_diff = 0;
	bool declared = false;
	pos = strstr(effect_text->array + main_diff, "{");
	if (pos) {
		size_t insert_diff = pos - effect_text->array + 1;
		dstr_insert(effect_text, insert_diff, " = float4(0.0,0.0,0.0,1.0);\n");
		dstr_insert(effect_text, insert_diff, var_name->array);
		dstr_insert(effect_text, insert_diff, "\n\tfloat4 ");
		declared = true;
		start_diff = insert_diff - main_diff + 37 + var_name->len;
	}

	pos = strstr(effect_text->array + main_diff + start_diff, var_name->array);
	while (pos) {
		size_t diff = pos - effect_text->array;
		char *ch = pos + var_name->len;
		bool part = false;
		if (*ch == '.') {
			part = true;
			ch++;
			while (is_var_char(*ch))
				ch++;
		}
		while (*ch == ' ' || *ch == '\t')
			ch++;

		if (*ch == '=') {
			replaced++;
			if (replaced == 1 && !declared) {
				if (part) {
					dstr_insert(effect_text, diff, " = float4(0.0,0.0,0.0,1.0);\n");
					dstr_insert(effect_text, diff, var_name->array);
					dstr_insert(effect_text, diff, "float4 ");
					diff += 35 + var_name->len;
				} else {
					dstr_insert(effect_text, diff, "float4 ");
					diff += 7;
				}
			} else if (replaced == count) {
				if (part) {
					while (*ch != ';' && *ch != 0)
						ch++;
					diff = ch - effect_text->array + 1;
					dstr_insert(effect_text, diff, ";");
					dstr_insert(effect_text, diff, var_name->array);
					dstr_insert(effect_text, diff, "\n\treturn ");
				} else {
					dstr_remove(effect_text, diff, ch - pos + 1);
					dstr_insert(effect_text, diff, "return ");
				}
				return;
			}
		} else if (*(ch + 1) == '=' && (*ch == '*' || *ch == '/' || *ch == '+' || *ch == '-')) {
			replaced++;
			if (replaced == 1 && !declared) {
				dstr_insert(effect_text, diff, " = float4(0.0,0.0,0.0,1.0);\n");
				dstr_insert(effect_text, diff, var_name->array);
				dstr_insert(effect_text, diff, "float4 ");
				diff += 35 + var_name->len;
			} else if (replaced == count) {
				while (*ch != ';' && *ch != 0)
					ch++;
				diff = ch - effect_text->array + 1;
				dstr_insert(effect_text, diff, ";");
				dstr_insert(effect_text, diff, var_name->array);
				dstr_insert(effect_text, diff, "\n\treturn ");
				return;
			}
		}

		pos = strstr(effect_text->array + diff + var_name->len, var_name->array);
	}
}

bool shader_filter_convert(obs_properties_t *props, obs_property_t *property, void *data)
{
	UNUSED_PARAMETER(props);
	if (!data)
		return false;
	struct shader_filter_data *filter = data;
	obs_data_t *settings = obs_source_get_settings(filter->context);
	if (!settings)
		return false;
	struct dstr effect_text = {0};
	dstr_init_copy(&effect_text, obs_data_get_string(settings, "shader_text"));

	//convert_define(&effect_text);

	size_t start_diff = 24;
	bool main_no_args = false;
	int uv = 0;
	char *main_pos = strstr(effect_text.array, "void mainImage(out vec4");
	if (!main_pos) {
		main_pos = strstr(effect_text.array, "void mainImage( out vec4");
		start_diff++;
	}
	if (!main_pos) {
		main_pos = strstr(effect_text.array, "void main()");
		if (main_pos)
			main_no_args = true;
	}
	if (!main_pos) {
		main_pos = strstr(effect_text.array, "vec4 effect(vec4");
		start_diff = 17;
		uv = 1;
	}

	if (!main_pos) {
		dstr_free(&effect_text);
		obs_data_release(settings);
		return false;
	}
	size_t main_diff = main_pos - effect_text.array;
	struct dstr return_color_name = {0};
	struct dstr coord_name = {0};
	if (main_no_args) {

		dstr_replace(&effect_text, "void main()", "float4 mainImage(VertData v_in) : TARGET");
		if (strstr(effect_text.array, "varying vec2 position;")) {
			uv = 1;
			dstr_init_copy(&coord_name, "position");
		} else if (strstr(effect_text.array, "varying vec2 pos;")) {
			uv = 1;
			dstr_init_copy(&coord_name, "pos");
		} else if (strstr(effect_text.array, "fNormal")) {
			uv = 2;
			dstr_init_copy(&coord_name, "fNormal");
		} else {
			uv = 0;
			dstr_init_copy(&coord_name, "gl_FragCoord");
		}

		char *out_start = strstr(effect_text.array, "out vec4");
		if (out_start) {
			char *start = out_start + 9;
			while (*start == ' ' || *start == '\t')
				start++;
			char *end = start;
			while (*end != ' ' && *end != '\t' && *end != '\n' && *end != ',' && *end != '(' && *end != ')' &&
			       *end != ';' && *end != 0)
				end++;
			dstr_ncat(&return_color_name, start, end - start);
			while (*end == ' ' || *end == '\t')
				end++;
			if (*end == ';')
				dstr_remove(&effect_text, out_start - effect_text.array, (end + 1) - out_start);
		} else {
			dstr_init_copy(&return_color_name, "gl_FragColor");
		}
	} else {

		char *start = main_pos + start_diff;
		while (*start == ' ' || *start == '\t')
			start++;
		char *end = start;
		while (*end != ' ' && *end != '\t' && *end != '\n' && *end != ',' && *end != ')' && *end != 0)
			end++;

		dstr_ncat(&return_color_name, start, end - start);

		start = strstr(end, ",");
		if (!start) {
			dstr_free(&effect_text);
			dstr_free(&return_color_name);
			obs_data_release(settings);
			return false;
		}
		start++;
		while (*start == ' ' || *start == '\t')
			start++;
		char *v2i = strstr(start, "in vec2 ");
		char *v2 = strstr(start, "vec2 ");
		char *close = strstr(start, ")");
		if (v2i && close && v2i < close) {
			start = v2i + 8;
		} else if (v2 && close && v2 < close) {
			start = v2 + 5;
		} else {
			if (*start == 'i' && *(start + 1) == 'n' && (*(start + 2) == ' ' || *(start + 2) == '\t'))
				start += 3;
			while (*start == ' ' || *start == '\t')
				start++;
			if (*start == 'v' && *(start + 1) == 'e' && *(start + 2) == 'c' && *(start + 3) == '2' &&
			    (*(start + 4) == ' ' || *(start + 4) == '\t'))
				start += 5;
		}
		while (*start == ' ' || *start == '\t')
			start++;

		end = start;
		while (*end != ' ' && *end != '\t' && *end != '\n' && *end != ',' && *end != ')' && *end != 0)
			end++;

		dstr_ncat(&coord_name, start, end - start);

		while (*end != ')' && *end != 0)
			end++;
		size_t idx = main_pos - effect_text.array;
		dstr_remove(&effect_text, idx, end - main_pos + 1);
		dstr_insert(&effect_text, idx, "float4 mainImage(VertData v_in) : TARGET");
	}

	convert_return(&effect_text, &return_color_name, main_diff);

	dstr_free(&return_color_name);

	if (dstr_cmp(&coord_name, "fragCoord") != 0) {
		if (dstr_find(&coord_name, "fragCoord")) {
			dstr_replace(&effect_text, coord_name.array, "fragCoord");
		} else {
			char *pos = strstr(effect_text.array, coord_name.array);
			while (pos) {
				size_t diff = pos - effect_text.array;
				if (((main_no_args || diff < main_diff) || diff > main_diff + 24) && !is_var_char(*(pos - 1)) &&
				    !is_var_char(*(pos + coord_name.len))) {
					dstr_remove(&effect_text, diff, coord_name.len);
					dstr_insert(&effect_text, diff, "fragCoord");
					pos = strstr(effect_text.array + diff + 9, coord_name.array);

				} else {
					pos = strstr(effect_text.array + diff + coord_name.len, coord_name.array);
				}
			}
		}
	}
	dstr_free(&coord_name);

	convert_if0(&effect_text);
	convert_if1(&effect_text);

	dstr_replace(&effect_text, "varying vec3", "//varying vec3");
	dstr_replace(&effect_text, "precision highp float;", "//precision highp float;");
	if (uv == 1) {
		dstr_replace(&effect_text, "fragCoord", "v_in.uv");
	} else if (uv == 2) {
		dstr_replace(&effect_text, "fragCoord.xy", "v_in.uv");
		dstr_replace(&effect_text, "fragCoord", "float3(v_in.uv,0.0)");
	} else {
		dstr_replace(&effect_text, "fragCoord.xy/iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "fragCoord.xy / iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "fragCoord/iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "fragCoord / iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "fragCoord", "(v_in.uv * uv_size)");
		dstr_replace(&effect_text, "gl_FragCoord.xy/iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "gl_FragCoord.xy / iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "gl_FragCoord/iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "gl_FragCoord / iResolution.xy", "v_in.uv");
		dstr_replace(&effect_text, "gl_FragCoord", "(v_in.uv * uv_size)");
	}
	dstr_replace(&effect_text, "love_ScreenSize", "uv_size");
	dstr_replace(&effect_text, "u_resolution", "uv_size");
	dstr_replace(&effect_text, "uResolution", "uv_size");
	dstr_replace(&effect_text, "iResolution.xyz", "float3(uv_size,0.0)");
	dstr_replace(&effect_text, "iResolution.xy", "uv_size");
	dstr_replace(&effect_text, "iResolution.x", "uv_size.x");
	dstr_replace(&effect_text, "iResolution.y", "uv_size.y");
	dstr_replace(&effect_text, "iResolution", "float4(uv_size,uv_pixel_interval)");

	dstr_replace(&effect_text, "iChannelResolution[0].xy", "uv_size");
	dstr_replace(&effect_text, "iChannelResolution[0]", "float3(uv_size, 1.0)");
	dstr_replace(&effect_text, "iChannelResolution[1].xy", "uv_size");
	dstr_replace(&effect_text, "iChannelResolution[1]", "float3(uv_size, 1.0)");

	dstr_replace(&effect_text, "uniform vec2 uv_size;", "");

	if (strstr(effect_text.array, "iTime"))
		dstr_replace(&effect_text, "iTime", "elapsed_time");
	else if (strstr(effect_text.array, "iGlobalTime"))
		dstr_replace(&effect_text, "iGlobalTime", "elapsed_time");
	else if (strstr(effect_text.array, "uTime"))
		dstr_replace(&effect_text, "uTime", "elapsed_time");
	else if (strstr(effect_text.array, "u_time"))
		dstr_replace(&effect_text, "u_time", "elapsed_time");
	else
		dstr_replace(&effect_text, "time", "elapsed_time");

	dstr_replace(&effect_text, "iTimeDelta", "delta_time");
	dstr_replace(&effect_text, "iFrame", "frame_count");

	dstr_replace(&effect_text, "uniform float elapsed_time;", "");
	dstr_replace(&effect_text, "uniform float delta_time;", "");
	dstr_replace(&effect_text, "uniform int frame_count;", "");

	dstr_replace(&effect_text, "iDate.w", "local_time");

	dstr_replace(&effect_text, "bvec2", "bool2");
	dstr_replace(&effect_text, "bvec3", "bool3");
	dstr_replace(&effect_text, "bvec4", "bool4");

	dstr_replace(&effect_text, "ivec4", "int4");
	dstr_replace(&effect_text, "ivec3", "int3");
	dstr_replace(&effect_text, "ivec2", "int2");

	dstr_replace(&effect_text, "uvec4", "uint4");
	dstr_replace(&effect_text, "uvec3", "uint3");
	dstr_replace(&effect_text, "uvec2", "uint2");

	dstr_replace(&effect_text, "vec4", "float4");
	dstr_replace(&effect_text, "vec3", "float3");
	dstr_replace(&effect_text, "vec2", "float2");

	dstr_replace(&effect_text, " number ", " float ");

	dstr_replace(&effect_text, "dFdx(", "ddx(");
	dstr_replace(&effect_text, "dFdy(", "ddy(");
	dstr_replace(&effect_text, "mix(", "lerp(");
	dstr_replace(&effect_text, "fract(", "frac(");
	dstr_replace(&effect_text, "inversesqrt(", "rsqrt(");

	dstr_replace(&effect_text, "iChannel0", "image");
	dstr_replace(&effect_text, "iChannel1", "previous_image");
	dstr_replace(&effect_text, "iChannel2", "previous_output");
	dstr_replace(&effect_text, "iChannel3", "image");

	dstr_replace(&effect_text, "extern bool", "uniform bool");
	dstr_replace(&effect_text, "extern uint", "uniform uint");
	dstr_replace(&effect_text, "extern int", "uniform int");
	dstr_replace(&effect_text, "extern float", "uniform float");

	convert_init(&effect_text, "bool");
	convert_init(&effect_text, "uint");
	convert_init(&effect_text, "int");
	convert_init(&effect_text, "float");

	convert_vector_init(&effect_text, "bool(", 1);
	convert_vector_init(&effect_text, "bool2(", 2);
	convert_vector_init(&effect_text, "bool3(", 3);
	convert_vector_init(&effect_text, "bool4(", 4);
	convert_vector_init(&effect_text, "uint(", 1);
	convert_vector_init(&effect_text, "uint2(", 2);
	convert_vector_init(&effect_text, "uint3(", 3);
	convert_vector_init(&effect_text, "uint4(", 4);
	convert_vector_init(&effect_text, "int(", 1);
	convert_vector_init(&effect_text, "int2(", 2);
	convert_vector_init(&effect_text, "int3(", 3);
	convert_vector_init(&effect_text, "int4(", 4);
	convert_vector_init(&effect_text, "float(", 1);
	convert_vector_init(&effect_text, "float2(", 2);
	convert_vector_init(&effect_text, "float3(", 3);
	convert_vector_init(&effect_text, "float4(", 4);
	convert_vector_init(&effect_text, "mat2(", 4);
	convert_vector_init(&effect_text, "mat3(", 9);
	convert_vector_init(&effect_text, "mat4(", 16);

	convert_atan(&effect_text);
	convert_mat_mul(&effect_text, "mat2");
	convert_mat_mul(&effect_text, "mat3");
	convert_mat_mul(&effect_text, "mat4");

	dstr_replace(&effect_text, "point(", "point2(");
	dstr_replace(&effect_text, "line(", "line2(");

	dstr_replace(&effect_text, "#version ", "//#version ");

	convert_if_defined(&effect_text);

	dstr_replace(&effect_text, "acos(-1.0)", "3.14159265359");
	dstr_replace(&effect_text, "acos(-1.)", "3.14159265359");
	dstr_replace(&effect_text, "acos(-1)", "3.14159265359");

	struct dstr insert_text = {0};
	dstr_init_copy(&insert_text, "#ifndef OPENGL\n");

	if (dstr_find(&effect_text, "mat2"))
		dstr_cat(&insert_text, "#define mat2 float2x2\n");
	if (dstr_find(&effect_text, "mat3"))
		dstr_cat(&insert_text, "#define mat3 float3x3\n");
	if (dstr_find(&effect_text, "mat4"))
		dstr_cat(&insert_text, "#define mat4 float4x4\n");

	if (dstr_find(&effect_text, "mod("))
		dstr_cat(&insert_text, "#define mod(x,y) ((x) - (y) * floor((x) / (y)))\n");
	if (dstr_find(&effect_text, "lessThan("))
		dstr_cat(&insert_text, "#define lessThan(a,b) ((a) < (b))\n");
	if (dstr_find(&effect_text, "greaterThan("))
		dstr_cat(&insert_text, "#define greaterThan(a,b) ((a) > (b))\n");
	dstr_cat(&insert_text, "#endif\n");

	if (dstr_find(&effect_text, "iMouse") && !dstr_find(&effect_text, "float2 iMouse"))
		dstr_cat(
			&insert_text,
			"uniform float4 iMouse<\nstring widget_type = \"slider\";\nfloat minimum=0.0;\nfloat maximum=1000.0;\nfloat step=1.0;\n>;\n");

	if (dstr_find(&effect_text, "iFrame") && !dstr_find(&effect_text, "float iFrame"))
		dstr_cat(&insert_text, "uniform float iFrame;\n");

	if (dstr_find(&effect_text, "iSampleRate") && !dstr_find(&effect_text, "float iSampleRate"))
		dstr_cat(&insert_text, "uniform float iSampleRate;\n");

	if (dstr_find(&effect_text, "iTimeDelta") && !dstr_find(&effect_text, "float iTimeDelta"))
		dstr_cat(&insert_text, "uniform float iTimeDelta;\n");

	int num_textures = 0;
	struct dstr texture_name = {0};
	struct dstr replacing = {0};
	struct dstr replacement = {0};

	char *texture_find[] = {"texture(", "texture2D(", "texelFetch(", "Texel(", "textureLod("};
	char *texture = NULL;
	size_t texture_diff = 0;
	for (size_t i = 0; i < sizeof(texture_find) / sizeof(char *); i++) {
		char *t = strstr(effect_text.array, texture_find[i]);
		if (t && (!texture || t < texture)) {
			texture = t;
			texture_diff = strlen(texture_find[i]);
		}
	}
	while (texture) {
		const size_t diff = texture - effect_text.array;
		if (is_var_char(*(texture - 1))) {
			texture = NULL;
			size_t prev_diff = texture_diff;
			for (size_t i = 0; i < 3; i++) {
				char *t = strstr(effect_text.array + diff + prev_diff, texture_find[i]);
				if (t && (!texture || t < texture)) {
					texture = t;
					texture_diff = strlen(texture_find[i]);
				}
			}
			continue;
		}
		char *start = texture + texture_diff;
		while (*start == ' ' || *start == '\t')
			start++;
		char *end = start;
		while (*end != ' ' && *end != '\t' && *end != ',' && *end != ')' && *end != '\n' && *end != 0)
			end++;
		dstr_copy(&texture_name, "");
		dstr_ncat(&texture_name, start, end - start);

		dstr_copy(&replacing, "");
		dstr_ncat(&replacing, texture, end - texture);

		dstr_copy(&replacement, "");

		if (num_textures) {
			dstr_cat_dstr(&replacement, &texture_name);
			dstr_cat(&replacement, ".Sample(textureSampler");

			dstr_cat(&insert_text, "uniform texture2d ");
			dstr_cat(&insert_text, texture_name.array);
			dstr_cat(&insert_text, ";\n");
		} else {
			dstr_cat(&replacement, "image.Sample(textureSampler");
		}
		dstr_replace(&effect_text, replacing.array, replacement.array);

		dstr_copy(&replacing, "textureSize(");
		dstr_cat(&replacing, texture_name.array);
		dstr_cat(&replacing, ", 0)");

		dstr_replace(&effect_text, replacing.array, "uv_size");
		dstr_replace(&replacing, ", 0)", ",0)");
		dstr_replace(&effect_text, replacing.array, "uv_size");

		num_textures++;
		size_t prev_diff = texture_diff;
		texture = NULL;
		for (size_t i = 0; i < sizeof(texture_find) / sizeof(char *); i++) {
			char *t = strstr(effect_text.array + diff + prev_diff, texture_find[i]);
			if (t && (!texture || t < texture)) {
				texture = t;
				texture_diff = strlen(texture_find[i]);
			}
		}
	}
	dstr_free(&replacing);
	dstr_free(&replacement);
	dstr_free(&texture_name);

	if (insert_text.len > 24) {
		dstr_insert_dstr(&effect_text, 0, &insert_text);
	}
	dstr_free(&insert_text);

	obs_data_set_string(settings, "shader_text", effect_text.array);

	dstr_free(&effect_text);

	obs_data_release(settings);
	obs_property_set_visible(property, false);

	filter->reload_effect = true;

	obs_source_update(filter->context, NULL);
	return true;
}
