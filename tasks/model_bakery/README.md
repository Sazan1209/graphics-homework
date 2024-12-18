# Запекалка моделей

В данном задании вам предстоит познакомиться с пайплайном работы с 3D-моделями.

Для этого мы будем использовать открытый формат хранения моделей [glTF](https://github.com/KhronosGroup/glTF), а также его расширение [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md).
Чтобы не заниматься парсингом и валидацией джейсонов в ручную, мы используем библиотеку [tinygltf](https://github.com/syoyo/tinygltf), предоставляющую удобный интерфейс для получения данных из glTF моделей.

С этим форматом вы уже могли столкнуться, если пробовали читать код семпла [shadowmap](/samples/shadowmap/).
Если вдруг вы изучали его особенно пристально, то могли заметить, что в классе [SceneManager](/common/scene/SceneManager.cpp) реализована достаточно длинная процедура перекодирования бинарных данных вершин и индексов из (почти) произвольного glTF-файла в удобный для рендеринга формат.
Такая процедура перекодирования, разумеется, занимает не нулевое время, и в крупной игре пагубно сказалась бы на времени загрузки карт или уровней.
Однако если вы делаете не игру, а какой-то инструмент для работы с произвольными моделями, то другого выхода у вас нет.

Вместо этого подхода в данном задании предлагается реализовать более разумный для игр подход: запекание моделей.
То есть просто заранее перекодировать модель в удобный для рендеринга формат отдельным приложением-печкой, а в приложении-рендерере загружать бинарные данные напрямую на GPU.

## Перед началом

 1. Откройте какую-нибудь простую модель текстовой версии формата glTF, например [SimpleMeshes.gltf](/resources/scenes/SimpleMeshes/glTF/SimpleMeshes.gltf), и разберитесь, что там написано при помощи [спецификации](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).
 2. Почитайте код [SceneManager](/common/scene/SceneManager.cpp).
 3. Убедитесь, что семпл в папке [renderer](./renderer/) у вас запускается.

## Задание

#### Шаг 1

Используя [tinygltf](https://github.com/syoyo/tinygltf), напишите в папке [baker](baker) приложение-запекалку, которое сможет перекодировать модели из папки [scenes](/resources/scenes/) в описанный далее формат.

Во-первых, все аттрибуты каждой вершины должны идти в буфере подряд.
Говоря простым языком, должно быть не $p_1, \dots p_N, n_1, \dots n_N$, а $p_1, n_1, \dots p_N, n_N$, где $p_i$ и $n_i$ это вектора позиции и нормали соответственно, а остальные аттрибуты опущены.
Во-вторых, нормали и касательные должны быть квантизованы до 8 бит на компонент.
Итого каждая вершина должна занимать 32 байта с учётом выравнивания и выглядеть в бинарном файле идущем с джейсоном модели следующим образом:
| Сдвиг (байты) | Содержимое    |
| ------------- | ------------- |
| 0  | Координаты, 3 по float32 |
| 12 | Нормаль, 3 байта |
| 15 | Паддинг, 1 байт
| 16 | Текстурные координаты, 2 по float32 |
| 24 | Касательная, 3 байта |
| 27 | Паддинг, 5 байт |

В качестве формата кодирования координаты в один байт возьмём равномерную сетку из 256 точек на отрезке $[-1, 1]$ и в байт будем писать номер ближайшей точки.
Иначе говоря, что-то вроде $[255\cdot0.5\cdot(x + 1)]$.

Наконец, индексы всегда будем хранить в формате `uint32_t`.

С одной стороны, коль скоро мы зафиксировали формат вершин, мы в принципе можем выкинуть часть json-файла модели описывающую этот формат.
С другой стороны, для дебага удобно всё таки иметь полноценную glTF модель которую можно открыть сторонней программой.
Поэтому приложение-запекалка также должна генерировать новый `.gltf` файл, описывающий этот формат при помощи расширения [KHR_mesh_quantization](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md).
Для генерации джейсонов используйте [tinygltf](https://github.com/syoyo/tinygltf).

Не забудьте проверить свою запекалку на нескольких моделях: [SimpleMeshes](/resources/scenes/SimpleMeshes/), [lovely_town](/resources/scenes/lovely_town/), [low_poly_dark_town](/resources/scenes/low_poly_dark_town/).

Обратите внимание, что в glTF касательные являются **четырёхмерными** векторами, требуя в последнюю координату записать ориентацию тройки базиса касательного пространства. Ясно, что для конкретного приложения совершенно не обязательно эту информацию хранить в каждой вершине и можно просто всегда использовать правую или левую тройку, получая битангент из векторного произведения. Однако, чтобы запечённые модели можно было открывать в сторонних программах, стоит соблюсти этот каприз glTF и записать после каждого тангента байтик четвёртой компоненты, содержащий число 1.

Также просьба написать запекалку таким образом, чтобы она принимала на вход 1 параметр, путь к `.gltf` модели, а результат клала в ту же папку, но с суффиксом `_baked`.
То есть при запуске `model_bakery_baker.exe D:/graphics-course/resources/scenes/lovely_town/scene.gltf` ваше приложение должно создавать в этой же папке файлы `scene_baked.gltf` и `scene_baked.bin`.

#### Шаг 2

Напишите в [SceneManager](/common/scene/SceneManager.hpp) новый (чтобы не сломать семплы) метод для загрузки моделей в сразу пригодном для рендеринга формате.
Этот метод должен напрямую загружать 2 половинки буфера glTF в вершинный и индексный буферы соответственно.

Поменяйте простой рендерер в папке [renderer](renderer) чтобы он использовал этот новый метод `SceneManager` и ожидал в шейдере описанный выше формат и проверьте, что всё работает.
Не забудьте расставить ассёртов на то, что рендерер запустили с ожидаемым им форматом модели.

Заметим, что код рендера скопирован с семпла [shadowmap](/samples/shadowmap/) и использует [октаэдральное кодирование нормалей](https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/).
К сожалению, glTF его не поддерживает в нужном нам виде даже с расширениями, так что от него придётся отказаться.

Данный рендерер мы будем в дальнейшем развивать и улучшать, так что смело меняйте его и подстраивайте так, как вам удобно.
Однако, пожалуйста, не пытайтесь излишне обобщать код.
Количество различных форматов моделей даже в ААА игре можно пересчитать по пальцам, писать супер-универсальные системы нужды практически нет.

## Бонусный уровень

### Вариант 1: best-fit normals

Вспомните, что такое [best-fit normals](https://knarkowicz.wordpress.com/2014/04/16/octahedron-normal-vector-encoding/).
Измените запекалку, чтобы она использовала этот метод.
Проверьте, что рендерер всё ещё работает.
Если он работает без изменений &mdash; почему? Если потребовались изменения &mdash; почему?

### Вариант 2: материалы

#### Шаг 1

Добавьте в [SceneManager](/common/scene/SceneManager.hpp) абстракцию материала: набора числовых констант и текстур, передаваемых в пиксельный шейдер при отрисовке.
Для начала в структуре материала поместим лишь 2 параметра: текстуру цвета поверхности и константу цвета поверхности.
Обратите внимание, что эти параметры взаимоисключающие: либо модель залита одним константным цветом, либо имеет текстуру.
Roughness, metallic и normals проигнорируем.
Каждому `RenderElement` должен соответствовать свой материал.
Также напишите код загрузки материалов и текстур из файла модели.

Методическая рекомендация: для айдишников материалов и текстур используйте типы вроде `enum class SomeId : uint32_t { Invalid = ~uint32_t{0}; };` вместо обычных интов.
Это позволит вам никогда не запутаться, где у вас какой конкретно айдишник и не передать случайно айди материала в функцию ожидающую айди текстуры.

#### Шаг 2

Поменяйте отрисовку чтобы она учитывала материал.
Константные параметры материалов мы будем передавать через юниформ-буферы, скажем, в нулевом биндинге, текстуру цвета через первый биндинг, а оставшиеся биндинги припасём для других видов текстур в будущем.
Однако, так как текстура и константный цвет &mdash; взаимоисключающие варианты для материала, нам придётся что-то придумать чтобы различать эти случаи.
На выбор предлагаются следующие варианты:

1. Сделайте два разных шейдера для константного цвета и цвета из текстуры.
2. Создайте "фейковых" текстур 1x1 пиксель и залейте их константными цветами используемыми вашими материалами.
   Для случая константного цвета используйте именно их.
3. Подключите в вулкане фичу `descriptorBindingPartiallyBound`.
   В случае константного цвета просто не биндите ничего в слот текстуры, а в юниформ-буфере заведите булевый флажок говорящий нужно ли читать текстуру или нет.
   **Обратите внимание**: тип `bool` на GPU занимает не 1 байт, а 4 байта!
   Возможно разумнее использовать для флажков битовую маску, так как в будущем их будет больше чем 1.

После выполнения посмотрите в профайлер и подумайте, как можно ускорить время отрисовки на CPU.
Возможно, стоит как-то группировать вызовы отрисовки с одинаковыми шейдерами и материалами?

## Полезные материалы

 1. https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html &mdash; спецификация glTF
 2. https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_mesh_quantization/README.md &mdash; расширение для квантизации данных в моделях
 3. https://github.khronos.org/glTF-Validator/ &mdash; официальный валидатор корректности glTF файлов
 4. https://github.khronos.org/glTF-Sample-Viewer-Release/ &mdash; официальный онлайн-просмотрщик glTF файлов
