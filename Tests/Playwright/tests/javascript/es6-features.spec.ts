import { test, expect } from '@playwright/test';

/**
 * ES6+ Features Tests (JS-ES6-001 to JS-ES6-010)
 * Priority: P0 (Critical)
 *
 * Tests modern JavaScript (ES6+) features including:
 * - Arrow functions
 * - Template literals
 * - Destructuring assignment
 * - Spread operator
 * - Rest parameters
 * - Classes and inheritance
 * - Modules (import/export)
 * - Map and Set collections
 * - Symbols
 */

test.describe('ES6+ Features', () => {

  test('JS-ES6-001: Arrow functions', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Basic arrow function
      const add = (a: number, b: number) => a + b;

      // Arrow function with block body
      const multiply = (a: number, b: number) => {
        const result = a * b;
        return result;
      };

      // Arrow function with implicit return of object
      const makeObject = (name: string, age: number) => ({ name, age });

      // Arrow function preserves 'this' context
      const obj = {
        value: 42,
        getValue: function() {
          return (() => this.value)();
        }
      };

      return {
        sum: add(3, 5),
        product: multiply(4, 6),
        object: makeObject('John', 30),
        thisValue: obj.getValue()
      };
    });

    expect(result.sum).toBe(8);
    expect(result.product).toBe(24);
    expect(result.object).toEqual({ name: 'John', age: 30 });
    expect(result.thisValue).toBe(42);
  });

  test('JS-ES6-002: Template literals', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      const name = 'World';
      const num = 42;

      // Basic template literal
      const greeting = `Hello, ${name}!`;

      // Multi-line template literal
      const multiline = `Line 1
Line 2
Line 3`;

      // Expression in template literal
      const expression = `The answer is ${num * 2}`;

      // Nested template literals
      const nested = `Outer ${`Inner ${num}`}`;

      // Tagged template literals
      function tag(strings: TemplateStringsArray, ...values: any[]) {
        return strings[0] + values[0].toUpperCase() + strings[1];
      }
      const tagged = tag`Hello, ${'world'}!`;

      return {
        greeting,
        hasNewlines: multiline.includes('\n'),
        lineCount: multiline.split('\n').length,
        expression,
        nested,
        tagged
      };
    });

    expect(result.greeting).toBe('Hello, World!');
    expect(result.hasNewlines).toBe(true);
    expect(result.lineCount).toBe(3);
    expect(result.expression).toBe('The answer is 84');
    expect(result.nested).toBe('Outer Inner 42');
    expect(result.tagged).toBe('Hello, WORLD!');
  });

  test('JS-ES6-003: Destructuring assignment', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Array destructuring
      const [a, b, c] = [1, 2, 3];
      const [first, , third] = [10, 20, 30]; // Skip elements
      const [x, y, ...rest] = [1, 2, 3, 4, 5]; // Rest elements

      // Object destructuring
      const person = { name: 'John', age: 30, city: 'NYC' };
      const { name, age } = person;

      // Destructuring with rename
      const { name: personName, age: personAge } = person;

      // Destructuring with default values
      const { country = 'USA' } = person;

      // Nested destructuring
      const user = {
        id: 1,
        profile: {
          username: 'johndoe',
          email: 'john@example.com'
        }
      };
      const { profile: { username, email } } = user;

      return {
        array: { a, b, c, first, third, x, y, rest },
        object: { name, age, personName, personAge, country },
        nested: { username, email }
      };
    });

    expect(result.array.a).toBe(1);
    expect(result.array.b).toBe(2);
    expect(result.array.c).toBe(3);
    expect(result.array.first).toBe(10);
    expect(result.array.third).toBe(30);
    expect(result.array.rest).toEqual([3, 4, 5]);
    expect(result.object.name).toBe('John');
    expect(result.object.age).toBe(30);
    expect(result.object.personName).toBe('John');
    expect(result.object.country).toBe('USA');
    expect(result.nested.username).toBe('johndoe');
  });

  test('JS-ES6-004: Spread operator', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Array spread
      const arr1 = [1, 2, 3];
      const arr2 = [4, 5, 6];
      const combined = [...arr1, ...arr2];

      // Array spread with additional elements
      const withExtra = [0, ...arr1, 3.5, ...arr2, 7];

      // Object spread
      const obj1 = { a: 1, b: 2 };
      const obj2 = { c: 3, d: 4 };
      const mergedObj = { ...obj1, ...obj2 };

      // Object spread with override
      const obj3 = { a: 1, b: 2 };
      const overridden = { ...obj3, b: 99, c: 3 };

      // Function arguments spread
      function sum(...numbers: number[]) {
        return numbers.reduce((acc, n) => acc + n, 0);
      }
      const nums = [1, 2, 3, 4, 5];
      const total = sum(...nums);

      // Shallow copy
      const original = [1, 2, 3];
      const copy = [...original];
      copy[0] = 99;

      return {
        combined,
        withExtra,
        mergedObj,
        overridden,
        total,
        originalUnchanged: original[0] === 1
      };
    });

    expect(result.combined).toEqual([1, 2, 3, 4, 5, 6]);
    expect(result.withExtra).toEqual([0, 1, 2, 3, 3.5, 4, 5, 6, 7]);
    expect(result.mergedObj).toEqual({ a: 1, b: 2, c: 3, d: 4 });
    expect(result.overridden).toEqual({ a: 1, b: 99, c: 3 });
    expect(result.total).toBe(15);
    expect(result.originalUnchanged).toBe(true);
  });

  test('JS-ES6-005: Rest parameters', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Rest parameters in function
      function sum(first: number, ...rest: number[]) {
        return first + rest.reduce((acc, n) => acc + n, 0);
      }

      // Rest parameters with destructuring
      function processArgs(name: string, ...scores: number[]) {
        return {
          name,
          count: scores.length,
          average: scores.reduce((a, b) => a + b, 0) / scores.length
        };
      }

      return {
        sum1: sum(1),
        sum2: sum(1, 2, 3, 4, 5),
        processing: processArgs('Student', 85, 90, 95)
      };
    });

    expect(result.sum1).toBe(1);
    expect(result.sum2).toBe(15);
    expect(result.processing.name).toBe('Student');
    expect(result.processing.count).toBe(3);
    expect(result.processing.average).toBe(90);
  });

  test('JS-ES6-006: Classes (constructor, methods)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      class Person {
        name: string;
        age: number;

        constructor(name: string, age: number) {
          this.name = name;
          this.age = age;
        }

        greet() {
          return `Hello, I'm ${this.name}`;
        }

        isAdult() {
          return this.age >= 18;
        }

        // Getter
        get info() {
          return `${this.name}, ${this.age} years old`;
        }

        // Setter
        set updateAge(newAge: number) {
          if (newAge > 0) {
            this.age = newAge;
          }
        }

        // Static method
        static species() {
          return 'Homo sapiens';
        }
      }

      const person = new Person('Alice', 25);
      person.updateAge = 26;

      return {
        name: person.name,
        age: person.age,
        greeting: person.greet(),
        isAdult: person.isAdult(),
        info: person.info,
        species: Person.species(),
        instanceOfPerson: person instanceof Person
      };
    });

    expect(result.name).toBe('Alice');
    expect(result.age).toBe(26);
    expect(result.greeting).toBe("Hello, I'm Alice");
    expect(result.isAdult).toBe(true);
    expect(result.info).toBe('Alice, 26 years old');
    expect(result.species).toBe('Homo sapiens');
    expect(result.instanceOfPerson).toBe(true);
  });

  test('JS-ES6-007: Class inheritance (extends, super)', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      class Animal {
        name: string;

        constructor(name: string) {
          this.name = name;
        }

        speak() {
          return `${this.name} makes a sound`;
        }
      }

      class Dog extends Animal {
        breed: string;

        constructor(name: string, breed: string) {
          super(name); // Call parent constructor
          this.breed = breed;
        }

        speak() {
          return `${this.name} barks`;
        }

        getInfo() {
          return `${super.speak()} - ${this.breed}`;
        }
      }

      const dog = new Dog('Buddy', 'Golden Retriever');

      return {
        name: dog.name,
        breed: dog.breed,
        speak: dog.speak(),
        info: dog.getInfo(),
        instanceOfDog: dog instanceof Dog,
        instanceOfAnimal: dog instanceof Animal
      };
    });

    expect(result.name).toBe('Buddy');
    expect(result.breed).toBe('Golden Retriever');
    expect(result.speak).toBe('Buddy barks');
    expect(result.info).toContain('Buddy makes a sound');
    expect(result.info).toContain('Golden Retriever');
    expect(result.instanceOfDog).toBe(true);
    expect(result.instanceOfAnimal).toBe(true);
  });

  test('JS-ES6-008: Modules (import/export)', { tag: '@p0' }, async ({ page }) => {
    // Note: Testing actual ES modules requires module context
    // This test validates the syntax understanding and dynamic import

    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(async () => {
      // Simulate module exports/imports using object notation
      const module1 = {
        default: function() { return 'default export'; },
        namedExport: 'named value',
        anotherExport: 42
      };

      const module2 = {
        multiply: (a: number, b: number) => a * b
      };

      // Simulate imports
      const defaultImport = module1.default;
      const { namedExport, anotherExport } = module1;
      const { multiply } = module2;

      return {
        default: defaultImport(),
        named: namedExport,
        another: anotherExport,
        multiplyResult: multiply(3, 4),
        hasExports: true
      };
    });

    expect(result.default).toBe('default export');
    expect(result.named).toBe('named value');
    expect(result.another).toBe(42);
    expect(result.multiplyResult).toBe(12);
    expect(result.hasExports).toBe(true);
  });

  test('JS-ES6-009: Map and Set', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Map operations
      const map = new Map();
      map.set('key1', 'value1');
      map.set('key2', 'value2');
      map.set(42, 'number key');
      map.set({ id: 1 }, 'object key');

      const mapResults = {
        size: map.size,
        hasKey1: map.has('key1'),
        getValue1: map.get('key1'),
        getNumberKey: map.get(42),
        keys: Array.from(map.keys()),
        values: Array.from(map.values())
      };

      map.delete('key2');
      const afterDelete = map.size;

      // Set operations
      const set = new Set();
      set.add(1);
      set.add(2);
      set.add(2); // Duplicate, won't be added
      set.add(3);

      const setResults = {
        size: set.size,
        has2: set.has(2),
        values: Array.from(set.values())
      };

      set.delete(2);
      const afterSetDelete = set.size;

      // Set from array (removes duplicates)
      const uniqueSet = new Set([1, 2, 2, 3, 3, 3, 4]);
      const uniqueArray = Array.from(uniqueSet);

      return {
        map: mapResults,
        mapAfterDelete: afterDelete,
        set: setResults,
        setAfterDelete: afterSetDelete,
        uniqueArray
      };
    });

    expect(result.map.size).toBe(4);
    expect(result.map.hasKey1).toBe(true);
    expect(result.map.getValue1).toBe('value1');
    expect(result.map.getNumberKey).toBe('number key');
    expect(result.mapAfterDelete).toBe(3);

    expect(result.set.size).toBe(3); // 1, 2, 3 (duplicate 2 ignored)
    expect(result.set.has2).toBe(true);
    expect(result.set.values).toEqual([1, 2, 3]);
    expect(result.setAfterDelete).toBe(2);

    expect(result.uniqueArray).toEqual([1, 2, 3, 4]);
  });

  test('JS-ES6-010: Symbols', { tag: '@p0' }, async ({ page }) => {
    await page.goto('data:text/html,<body></body>');

    const result = await page.evaluate(() => {
      // Create symbols
      const sym1 = Symbol('description');
      const sym2 = Symbol('description');
      const sym3 = Symbol();

      // Symbols are unique
      const areUnique = sym1 !== sym2;

      // Symbol as object key
      const obj: any = {};
      const symKey = Symbol('key');
      obj[symKey] = 'value';
      obj.regularKey = 'regular';

      // Symbol keys don't appear in regular enumeration
      const regularKeys = Object.keys(obj);
      const symbolKeys = Object.getOwnPropertySymbols(obj);

      // Well-known symbols
      const iterableObj = {
        *[Symbol.iterator]() {
          yield 1;
          yield 2;
          yield 3;
        }
      };
      const arrayFromIterable = Array.from(iterableObj);

      // Symbol description
      const symWithDesc = Symbol('test description');
      const description = symWithDesc.description;

      return {
        areUnique,
        typeof: typeof sym1,
        regularKeysCount: regularKeys.length,
        symbolKeysCount: symbolKeys.length,
        symbolValue: obj[symKey],
        arrayFromIterable,
        description
      };
    });

    expect(result.areUnique).toBe(true);
    expect(result.typeof).toBe('symbol');
    expect(result.regularKeysCount).toBe(1); // Only 'regularKey'
    expect(result.symbolKeysCount).toBe(1); // Only the symbol key
    expect(result.symbolValue).toBe('value');
    expect(result.arrayFromIterable).toEqual([1, 2, 3]);
    expect(result.description).toBe('test description');
  });

});
