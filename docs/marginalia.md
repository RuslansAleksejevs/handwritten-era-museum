[← Museum](../README.md) · [Timeline](timeline.md) · [Origins](origins.md)

# Notes from the margins

Original comments by Ruslan Aleksejevs, selected from personal repository READMEs, code comments and notebooks. The Russian text keeps its original spelling and profanity; the English translations were added for this edition. These are historical remarks, not current technical advice.

## On leaving comments for future me

> Нда, нужно было писать комменты тогда, зря торопился

*Yeah, I should have written comments back then. Shouldn't have rushed.*

Source: music archive / README.md, line 5.

## On contest problem ordering

> Нельзя давать такую хуету в качестве Q1.

*You can't give this shit as Q1.*

Source: algorithms archive / contests/biweekly100.ipynb, notebook cell 4 (zero-based).

## On a very optimistic maintenance schedule

> И ВООБЩЕ, ЧТО-ТО ЭТА ШТУКА ПЕРЕСТАЛА РАБОТАТЬ, ИСПРАВЬ НА ДНЯХ

*AND ANYWAY, THIS THING HAS SOMEHOW STOPPED WORKING. FIX IT IN THE NEXT FEW DAYS.*

Source: learning archive / README.md, line 28.

## On recursion aesthetics

> Хотя рекурсивные внешне пизже, офк.

*Though the recursive versions look fucking better, of course.*

Source: algorithms archive / README.md, line 41.

## On discovering a useful tool

> охуенная штука, обожаю.

*Fucking great thing. Love it.* The tool was hyperopt-sklearn.

Source: learning archive / README.md, line 33.

## On finally solving the problem

> Решил, так и есть, точно идет в мой топ.

*Solved it. Yes, exactly. Definitely one of my favourites.* The problem was LeetCode 746, Min Cost Climbing Stairs.

Source: algorithms archive / README.md, line 72.

The excerpts are self-contained; private archive names and URLs are omitted. [Source fingerprints](marginalia-sources.json).

## More notes, selected by the author

### On postponing a proof

> Доказывать последнее равенство классов мне в лом.

*I can't be bothered to prove that last equality of classes.*

Source: algorithms archive, biweekly100.ipynb, cell 19 (0-based). Selection 1; [fingerprint](marginalia-sources.json).

### On an unhelpful feature

> я был молод и глуп, id сам по себе обьект никак не описывает.

*I was young and stupid; an ID by itself tells you nothing about an object.*

Source: machine learning archive, ML_final_project.ipynb, cell 4 (0-based). Selection 4; [fingerprint](marginalia-sources.json).

### On missing pointers

> Без указателей что-то сложно.

*Things are kind of difficult without pointers.*

Source: algorithms archive, README.md, line 27. Selection 11; [fingerprint](marginalia-sources.json).

### On sharing a matrix

> Указатели, потому что мои потоки должны именно менять матрицу!!!

*Pointers, because my threads need to actually change the matrix!!!*

Source: numerical methods archive, solver.h, line 20. Selection 13; [fingerprint](marginalia-sources.json).

### On generalising for fun

> вообще, пригодится только для векторов, но хохмы ради определим для любой матрицы

*Actually, we'll only need it for vectors, but let's define it for any matrix just for fun.*

Source: numerical methods archive, matrix.h, line 61. Selection 14; [fingerprint](marginalia-sources.json).

### On choosing a memory layout

> Делаю это осознанно, чтобы легче через MPI_Bcast() передавать сразу столбец целиком

*I'm doing this deliberately so MPI_Bcast() can send the whole column at once more easily.*

Source: numerical methods archive, matrix.h, line 40. Selection 16; [fingerprint](marginalia-sources.json).

### On checking the textbook

> if очень важен, иначе появится нулевая строка, но богачеву в книжке пофиг, его прога не работала бы

*The if is essential; otherwise you get a zero row, but Bogachev's book doesn't care — his program wouldn't work.*

Source: numerical methods archive, solver.h, line 32. Selection 17; [fingerprint](marginalia-sources.json).

### On a recursion worth keeping

> банальная, но не очень банальная рекурсия, стоит ее отметить.

*A trivial, but not quite trivial, recursion. Worth making a note of.*

Source: algorithms archive, README.md, line 79. Selection 20; [fingerprint](marginalia-sources.json).

### On less fashionable algorithms

> хочу по разным экзотическим сортировкам постепенно пройтись, не только по меинстримным мердж и квик

*I want to gradually work through some exotic sorts, not just mainstream merge and quicksort.*

Source: algorithms archive, README.md, line 86. Selection 21; [fingerprint](marginalia-sources.json).

### On spotting a recurrence

> Тупо замечаю рекуррентное соотношение, откуда все сразу решается.

*Just spot the recurrence, and everything follows immediately.*

Source: algorithms archive, biweekly99.ipynb, cell 7 (0-based). Selection 24; [fingerprint](marginalia-sources.json).

### On letting code explain

> По-быстрому набросал код, из него все ясно.

*Quickly threw together the code; it makes everything clear.*

Source: algorithms archive, biweekly100.ipynb, cell 14 (0-based). Selection 25; [fingerprint](marginalia-sources.json).

### On discovering a mathematical solution

> Логика то же самая, что у меня, но тут я ее более аккуратно расписываю и оказывается, что задача не на прогу.

*It's the same logic as mine, but here I write it out more carefully and it turns out this isn't really a programming problem.*

Source: algorithms archive, biweekly100.ipynb, cell 11 (0-based). Selection 26; [fingerprint](marginalia-sources.json).

### On keeping training separate

> Но вообще я ведь должен обучать скейлер только на трейне?

*But shouldn't I fit the scaler only on the training data?*

Source: machine learning archive, NN4science_5_neural_networks.ipynb, cell 23 (0-based). Selection 31; [fingerprint](marginalia-sources.json).

### On understanding the order

> сортирую, как надо (и разбираюсь, почему так надо).

*Sorting it the right way (and figuring out why that is the right way).*

Source: machine learning archive, ML_final_project.ipynb, cell 7 (0-based). Selection 32; [fingerprint](marginalia-sources.json).

### On getting the idea

> уловил привкус, а остальное сам додумывал.

*Got a taste of the idea and worked out the rest myself.*

Source: algorithms archive, biweekly99.ipynb, cell 21 (0-based). Selection 36; [fingerprint](marginalia-sources.json).

### On a satisfying struggle

> Потно, но задача отличная.

*A slog, but a great problem.*

Source: algorithms archive, biweekly99.ipynb, cell 21 (0-based). Selection 37; [fingerprint](marginalia-sources.json).

### On what recursion does with memory

> Типа наша память юзается, как вспомогательная хуйня для рекурсии.

*Like, our memory is being used as auxiliary shit for the recursion.*

Source: algorithms archive, README.md, line 114. Selection 38; [fingerprint](marginalia-sources.json).

### On finding a pattern in pure noise

> Фигасе, реааально сильно коррелированы!

*Whoa, they really are strongly correlated!*

Source: feature-engineering notebook, NN4science_4_feature_engineering.ipynb, cell 11 (zero-based). [Original cells and plots](../originals/learning/random-correlations.ipynb) · [The experiment and its lesson](../projects/machine-learning/random-correlations/README.md) · [Fingerprint](marginalia-sources.json).
