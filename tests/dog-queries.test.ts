import { test, describe, before } from "node:test";
import assert from "node:assert";
import { getDogs } from "../src/lib/dog-queries";

function createMockSupabase() {
    const mockQuery = {
      select: function (arg) { this.select.calledWith = arg; return this; },
      eq: function (col, val) { this.eq.calledWith.push({col, val}); return this; },
      or: function (arg) { this.or.calledWith = arg; return this; },
      order: function (col, opts) { this.order.calledWith.push({col, opts}); return this; },
      ilike: function (col, val) { return this; },
      range: function (from, to) { this.range.calledWith = {from, to}; return this; },
    };

    mockQuery.select.calledWith = '';
    mockQuery.eq.calledWith = [];
    mockQuery.or.calledWith = '';
    mockQuery.order.calledWith = [];
    mockQuery.range.calledWith = {};
  
    const mockSupabase = {
        from: (tableName) => {
          mockSupabase.from.calledWith = tableName;
          return mockQuery;
        },
      };
    mockSupabase.from.calledWith = '';

    return { mockSupabase, mockQuery };
}

describe("getDogs query builder", () => {

  test("should apply state filter correctly", async () => {
    const { mockSupabase, mockQuery } = createMockSupabase();
    const params = new URLSearchParams("?state=MA");
    await getDogs(mockSupabase, params);

    assert.strictEqual(mockSupabase.from.calledWith, "dogs");
    assert.ok(mockQuery.eq.calledWith.some(c => c.col === 'is_available' && c.val === true));
    assert.strictEqual(mockQuery.or.calledWith, "state_code.eq.MA,shelters.state_code.eq.MA");
  });

  test("should apply sorting correctly", async () => {
    const { mockSupabase, mockQuery } = createMockSupabase();
    const params = new URLSearchParams("?sort=newest");
    await getDogs(mockSupabase, params);

    assert.ok(mockQuery.order.calledWith.some(c => c.col === 'created_at'));
  });
  
  test("should apply pagination correctly", async () => {
    const { mockSupabase, mockQuery } = createMockSupabase();
    const params = new URLSearchParams("?page=3&limit=20");
    await getDogs(mockSupabase, params);

    assert.deepStrictEqual(mockQuery.range.calledWith, { from: 40, to: 59 });
  });

});
