#include "include/visitor.h"
#include "include/fjb.h"
#include "include/gc.h"
#include "include/gen.h"
#include "include/io.h"
#include "include/node.h"
#include "include/resolve.h"
#include "include/string_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern gc_T* GC;
extern list_T* defs;

static AST_T* getptr_any(AST_T* ast, list_T* stack)
{
  if (!ast->name)
    return 0;

  AST_T* ptr = 0;

  if (ast->node)
    ptr = ast->node;

  if (!ptr)
    ptr = get_node_by_name(stack, ast->name);

  if (ptr && ptr->node)
    ptr = ptr->node;
  if (ptr && ptr->type == AST_ASSIGNMENT && ptr->value)
    return ptr->value;

  return ptr;
}

static AST_T* getptr(AST_T* ast, list_T* stack)
{
  return 0;
  if (!ast)
    return 0;
  if (!ast->name)
    return 0;
  if (!stack)
    return 0;
  if (!stack->size)
    return 0;

  AST_T* ptr = 0;

  list_T* list = stack;

  if (ast->ptr && ast->ptr->type == AST_OBJECT && ast->ptr->name) {
    if (strcmp(ast->ptr->name, "module") == 0 || strcmp(ast->ptr->name, "exports") == 0) {
      list = ast->ptr->list_value;

      if (list->size == 0)
        return ast->ptr;
    }
  }

  ptr = getptr_any(ast, list);

  return ptr;
}

visitor_T* init_visitor(parser_T* parser, const char* filepath, list_T* imports, AST_T* module,
                        AST_T* exports)
{
  visitor_T* visitor = calloc(1, sizeof(struct FJB_VISITOR_STRUCT));
  visitor->parser = parser;
  visitor->filepath = filepath;
  visitor->imports = imports;
  visitor->es_exports = init_list(sizeof(AST_T*));
  visitor->module = module;
  visitor->exports = exports;

  return visitor;
}

static list_T* visit_array(visitor_T* visitor, list_T* list, list_T* stack)
{
  if (list) {
    for (unsigned int i = 0; i < list->size; i++) {
      AST_T* child = (AST_T*)list->items[i];
      if (!child)
        continue;

      list->items[i] = visitor_visit(visitor, child, stack);
    }
  }

  return list;
}

AST_T* visitor_visit_id(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_int(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_float(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_string(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_import(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  char* final_file_to_read = resolve_import((char*)visitor->filepath, ast->string_value);

  for (unsigned int i = 0; i < ast->list_value->size; i++) {
    list_push(visitor->imports, ast->list_value->items[i]);
  }

  char* contents = fjb_read_file(final_file_to_read);
  gen_flags_T flags;
  flags.filepath = final_file_to_read;
  compiler_result_T* result = fjb(flags, contents, visitor->imports);
  ast->compiled_value = strdup(result->stdout);
  ast->es_exports = result->es_exports;
  ast->stack_frame = list_copy(stack);

  free(final_file_to_read);
  free(contents);

  compiler_result_free(result);

  list_clear(visitor->imports);

  return ast;
}

AST_T* visitor_visit_assignment(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->flags)
    ast->flags = visit_array(visitor, ast->flags, stack);
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->value)
    ast->value = visitor_visit(visitor, ast->value, stack);

  if (!ast->name && ast->left && ast->left->name)
    ast->name = strdup(ast->left->name);

  if (ast->value && ast->left) {
    AST_T* leftptr = ast->left->ptr;
    AST_T* rightptr = ast->value->ptr ? ast->value->ptr : ast->value;
    char* name = ast->left->type == AST_BINOP ? ast->left->right->name : ast->left->name;

    if (leftptr && rightptr) {
      if (leftptr->type == AST_OBJECT && leftptr->list_value) {
        AST_T* assignment = init_assignment(name, rightptr);
        list_push(leftptr->list_value, assignment);
      } else {
        if (leftptr->value)
          leftptr->value = rightptr;
        else
          ast->left->ptr = rightptr;
      }
    }
  }

  list_push_safe(stack, ast);

  return ast;
}

AST_T* visitor_visit_state(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (strcmp(ast->name, "export") == 0 && ast->value) {
    list_push_safe(visitor->es_exports, ast->value);
  }

  if (ast->value) {
    visitor_visit(visitor, ast->value, stack);
  }

  return ast;
}

AST_T* visitor_visit_try(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_condition(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_do(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_while(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_for(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_switch(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_array(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_regex(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_object(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_function(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  unsigned int stack_before = stack->size;

  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  if (ast->list_value) {
    for (unsigned int i = 0; i < ast->list_value->size; i++) {
      AST_T* child = (AST_T*)ast->list_value->items[i];
      list_push_safe(stack, child);
    }
  }

  if (ast->name) {
    list_push_safe(stack, ast);
  }

  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);

  unsigned int new_size = stack->size;

  if (ast->name && new_size > stack_before) {
    for (unsigned int i = new_size - 1; i > stack_before; i--) {
      AST_T* child = (AST_T*)stack->items[i];
      if (!child)
        continue;

      list_remove(stack, child, 0);
    }
  }

  return ast;
}

AST_T* visitor_visit_signature(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_compound(visitor_T* visitor, AST_T* ast, list_T* stack)
{

  if (ast->list_value) {
    for (unsigned int i = 0; i < ast->list_value->size; i++) {
      AST_T* child = (AST_T*)ast->list_value->items[i];
      if (!child)
        continue;

      ast->list_value->items[i] = visitor_visit(visitor, child, stack);

      if (child->type == AST_STATE && child->token && child->token->type == TOKEN_RETURN) {
        ast->ptr = child->value;
      }
    }
  }

  ast->stack_frame = list_copy(stack);

  return ast;
}

AST_T* visitor_visit_ternary(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->value)
    ast->value = visitor_visit(visitor, ast->value, stack);

  return ast;
}

AST_T* visitor_visit_name(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left) {
    ast->left = visitor_visit(visitor, ast->left, stack);
  }

  if (ast->right) {
    ast->right = visitor_visit(visitor, ast->right, stack);
  }

  if (ast->flags)
    ast->flags = visit_array(visitor, ast->flags, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  ast->ptr = getptr(ast, stack);

  return ast;
}

AST_T* visitor_visit_arrow_definition(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->value)
    ast->value = visitor_visit(visitor, ast->value, stack);
  if (ast->body)
    ast->body = visitor_visit(visitor, ast->body, stack);
  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit_colon_assignment(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);
  if (ast->value)
    ast->value = visitor_visit(visitor, ast->value, stack);
  if (ast->label_value)
    ast->label_value = visitor_visit(visitor, ast->label_value, stack);
  if (ast->expr)
    ast->expr = visitor_visit(visitor, ast->expr, stack);

  return ast;
}

AST_T* visitor_visit_increment(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);

  return ast;
}

AST_T* visitor_visit_decrement(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);

  return ast;
}

AST_T* visitor_visit_call(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);

  if (ast->list_value)
    ast->list_value = visit_array(visitor, ast->list_value, stack);

  if (ast->left && ast->left->name && strcmp(ast->left->name, "require") == 0) {
    char* str = 0;
    for (unsigned int i = 0; i < ast->list_value->size; i++) {
      AST_T* child_ast = (AST_T*)ast->list_value->items[i];

      if (child_ast->type == AST_STRING) {
        if (!is_builtin_module(child_ast->string_value)) {
          str = strdup(child_ast->string_value);
        }
      }
    }

    if (str != 0) {
      char* final_file_to_read = resolve_import((char*)visitor->parser->filepath, str);

      if (final_file_to_read) {
        char* contents = fjb_read_file(final_file_to_read);
        gen_flags_T flags;
        flags.filepath = final_file_to_read;
        compiler_result_T* result = fjb(flags, contents, visitor->imports);
        ast->compiled_value = strdup(result->stdout);
        ast->compiled_value = ast->compiled_value;
        ast->node = result->node;
        ast->ptr = result->node;
        // gc_mark(GC, ast->left);
        ast->left = 0;
        free(contents);
        compiler_result_free(result);
        free(final_file_to_read);
      }

      free(str);
    }
  }

  list_push_safe(stack, ast);

  return ast;
}

AST_T* visitor_visit_binop(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);

  if (ast->left && ast->left->ptr) {
    ast->right->ptr = ast->left->ptr;
  }

  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);

  if (ast->right->ptr) {
    ast->ptr = ast->right->ptr;
  }

  return ast;
}

AST_T* visitor_visit_undefined(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_noop(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_unop(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (ast->left)
    ast->left = visitor_visit(visitor, ast->left, stack);
  if (ast->right)
    ast->right = visitor_visit(visitor, ast->right, stack);

  return ast;
}

AST_T* visitor_visit_hex(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  return ast;
}

AST_T* visitor_visit_tuple(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  ast->list_value = visit_array(visitor, ast->list_value, stack);

  return ast;
}

AST_T* visitor_visit(visitor_T* visitor, AST_T* ast, list_T* stack)
{
  if (!ast) {
    printf("[Visitor]: Encountered %p AST.\n", ast);
    exit(1);
  }

  switch (ast->type) {
    case AST_ARRAY: ast = visitor_visit_array(visitor, ast, stack); break;
    case AST_HEX: ast = visitor_visit_hex(visitor, ast, stack); break;
    case AST_INT: ast = visitor_visit_int(visitor, ast, stack); break;
    case AST_FLOAT: ast = visitor_visit_float(visitor, ast, stack); break;
    case AST_STRING: ast = visitor_visit_string(visitor, ast, stack); break;
    case AST_ARROW_DEFINITION: ast = visitor_visit_arrow_definition(visitor, ast, stack); break;
    case AST_ASSIGNMENT: ast = visitor_visit_assignment(visitor, ast, stack); break;
    case AST_COLON_ASSIGNMENT: ast = visitor_visit_colon_assignment(visitor, ast, stack); break;
    case AST_WHILE: ast = visitor_visit_while(visitor, ast, stack); break;
    case AST_FOR: ast = visitor_visit_for(visitor, ast, stack); break;
    case AST_COMPOUND: ast = visitor_visit_compound(visitor, ast, stack); break;
    case AST_IMPORT: ast = visitor_visit_import(visitor, ast, stack); break;
    case AST_UNDEFINED: ast = visitor_visit_undefined(visitor, ast, stack); break;
    case AST_CALL: ast = visitor_visit_call(visitor, ast, stack); break;
    case AST_FUNCTION: ast = visitor_visit_function(visitor, ast, stack); break;
    case AST_SIGNATURE: ast = visitor_visit_signature(visitor, ast, stack); break;
    case AST_NAME: ast = visitor_visit_name(visitor, ast, stack); break;
    case AST_STATE: ast = visitor_visit_state(visitor, ast, stack); break;
    case AST_BINOP: ast = visitor_visit_binop(visitor, ast, stack); break;
    case AST_UNOP: ast = visitor_visit_unop(visitor, ast, stack); break;
    case AST_INCREMENT: ast = visitor_visit_increment(visitor, ast, stack); break;
    case AST_DECREMENT: ast = visitor_visit_decrement(visitor, ast, stack); break;
    case AST_REGEX: ast = visitor_visit_regex(visitor, ast, stack); break;
    case AST_OBJECT: ast = visitor_visit_object(visitor, ast, stack); break;
    case AST_CONDITION: ast = visitor_visit_condition(visitor, ast, stack); break;
    case AST_SWITCH: ast = visitor_visit_switch(visitor, ast, stack); break;
    case AST_TRY: ast = visitor_visit_try(visitor, ast, stack); break;
    case AST_TERNARY: ast = visitor_visit_ternary(visitor, ast, stack); break;
    case AST_DO: ast = visitor_visit_do(visitor, ast, stack); break;
    case AST_NOOP: ast = visitor_visit_noop(visitor, ast, stack); break;
    case AST_TUPLE: ast = visitor_visit_tuple(visitor, ast, stack); break;
    default: {
    } break;
  }

  if (ast->next)
    visitor_visit(visitor, ast->next, stack);

  return ast;
}

void visitor_free(visitor_T* visitor)
{
  free(visitor);
}
